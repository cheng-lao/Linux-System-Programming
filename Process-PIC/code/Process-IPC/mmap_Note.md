# mmap 内存映射详解笔记

## 目录

- [1. mmap 函数概述](#1-mmap-函数概述)
- [2. mmap 的优势](#2-mmap-的优势)
- [3. 共享内存映射的原理](#3-共享内存映射的原理)
- [4. MAP_SHARED vs MAP_PRIVATE](#4-map_shared-vs-map_private)
- [5. 写时复制（COW）机制详解](#5-写时复制cow机制详解)
- [6. 映射区是否相同？](#6-映射区是否相同)
- [7. 父子进程通信](#7-父子进程通信)
- [8. 无血缘关系进程通信](#8-无血缘关系进程通信)
- [9. /dev/zero 与匿名映射](#9-devzero-与匿名映射)
- [10. 使用注意事项](#10-使用注意事项)
- [11. 保险调用方式](#11-保险调用方式)
- [12. 实际应用示例](#12-实际应用示例)

---

## 1. mmap 函数概述

### 1.1 函数原型

```c
#include <sys/mman.h>

void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
```

### 1.2 参数说明

| 参数 | 类型 | 说明 |
|------|------|------|
| `addr` | `void *` | 建议的映射起始地址，通常设为 `NULL` 让系统自动选择 |
| `length` | `size_t` | 映射的字节数，通常需要是页大小的整数倍（4KB） |
| `prot` | `int` | 内存保护标志：`PROT_READ`、`PROT_WRITE`、`PROT_EXEC`、`PROT_NONE` |
| `flags` | `int` | 映射标志：`MAP_SHARED`、`MAP_PRIVATE`、`MAP_ANONYMOUS` 等 |
| `fd` | `int` | 文件描述符，匿名映射时设为 `-1` |
| `offset` | `off_t` | 文件偏移量，必须是页大小的整数倍（4096 的倍数） |

### 1.3 返回值

- **成功**：返回映射区域的起始地址
- **失败**：返回 `MAP_FAILED`，并设置 `errno`

### 1.4 取消映射

```c
int munmap(void *addr, size_t length);
```

- `addr`：必须是 `mmap` 返回的原始地址
- `length`：必须是映射时的大小

---

## 2. mmap 的优势

### 2.1 性能优势（最重要）

#### 传统方式（read/write）：

```
数据流：用户空间 → 内核缓冲区 → 页缓存 → 磁盘
        ↑ 需要系统调用，数据拷贝
```

- 每次读写都需要系统调用（用户态 ↔ 内核态切换）
- 数据需要在内核缓冲区中转
- 有两次数据拷贝：用户空间 → 内核 → 用户空间

#### mmap 方式：

```
数据流：页缓存 ↔ 用户空间（直接访问）
        ↑ 零拷贝，直接内存访问
```

- **零拷贝**：数据直接在共享内存中，无需内核中转
- **无需系统调用**：像操作普通内存一样
- **性能提升**：比管道快 10-100 倍（取决于数据量）

### 2.2 灵活性和易用性

#### 随机访问能力：

```c
// 管道：只能顺序读写
read(fd, buf, 100);   // 读取前100字节
read(fd, buf, 100);   // 读取接下来100字节（不能回头！）

// mmap：可以随机访问
char *p = mmap(...);
p[0] = 'A';           // 访问第一个字节
p[1000] = 'B';       // 直接跳到第1000个字节
p[500] = 'C';        // 可以回头修改！
```

#### 像操作普通内存一样：

```c
char *p = mmap(...);

// 可以使用所有标准C函数
strcpy(p, "hello");
strcat(p, " world");
sprintf(p, "value: %d", 42);

// 可以使用指针运算
char *ptr = p + 100;
*ptr = 'X';

// 可以使用结构体
struct Data {
    int id;
    char name[100];
};
struct Data *data = (struct Data*)p;
data->id = 1;
strcpy(data->name, "test");
```

### 2.3 与其他 IPC 方式对比

| 特性 | 管道/FIFO | 消息队列 | 共享内存映射 |
|------|-----------|----------|--------------|
| 速度 | 慢（需要系统调用） | 中等 | **快（直接内存访问）** |
| 数据拷贝次数 | 2次 | 2次 | **0次（零拷贝）** |
| 容量限制 | 有限（通常64KB） | 有限 | **大（可映射GB级）** |
| 使用方式 | 流式（顺序读写） | 消息队列 | **随机访问（像数组）** |
| 同步机制 | 自动阻塞 | 自动阻塞 | 需要手动同步（信号量/互斥锁） |

---

## 3. 共享内存映射的原理

### 3.1 核心概念：什么是"映射"？

**映射不是把文件内容复制到内存，而是建立虚拟地址与文件/物理内存的对应关系。**

```
传统方式（read/write）：
文件 → [内核缓冲区] → 用户空间内存
      ↑ 需要拷贝数据

mmap 方式（映射）：
文件 ←→ [页缓存] ←→ 虚拟地址空间
      ↑ 只是建立"指针"关系，不拷贝数据！
```

### 3.2 虚拟内存与物理内存的关系

#### 每个进程都有独立的虚拟地址空间：

```
进程A的虚拟地址空间        进程B的虚拟地址空间
┌─────────────────┐      ┌─────────────────┐
│  0x00000000      │      │  0x00000000      │
│  ...             │      │  ...             │
│  0x40000000      │      │  0x40000000      │  ← 相同的虚拟地址
│  (mmap区域)      │      │  (mmap区域)      │
│  0x80000000      │      │  0x80000000      │
└─────────────────┘      └─────────────────┘
         │                       │
         └───────────┬───────────┘
                     │
              ┌──────▼──────┐
              │  页表(MMU)  │  ← 地址转换
              └──────┬──────┘
                     │
         ┌───────────┴───────────┐
         │                       │
    ┌────▼────┐            ┌─────▼─────┐
    │物理内存 │            │ 页缓存    │
    │         │            │(文件数据) │
    └─────────┘            └───────────┘
```

**关键点：**
- 不同进程可以使用相同的虚拟地址
- 通过页表（MMU）转换为物理地址
- 多个进程可以映射到同一块物理内存（页缓存）

### 3.3 mmap 的映射过程（详细步骤）

#### 步骤1：调用 mmap()

```c
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 0);
```

**此时发生了什么？**

```
1. 内核在进程的虚拟地址空间中分配一段虚拟地址（比如 0x40000000）
2. 内核在页表中创建映射条目，但此时物理页还未分配
3. 返回虚拟地址给用户程序
```

#### 步骤2：首次访问（触发缺页中断）

```c
strcpy(p, "hello");  // 第一次写入
```

**此时发生了什么？**

```
1. CPU 访问虚拟地址 0x40000000
2. MMU 查页表，发现该页还没有物理页（页表项标记为"不存在"）
3. 触发缺页中断（Page Fault）
4. 内核处理缺页中断：
   a. 从文件系统读取对应页的数据到页缓存（Page Cache）
   b. 分配物理内存页（或使用页缓存中的页）
   c. 更新页表：虚拟地址 → 物理地址
5. 恢复用户程序执行，现在可以正常访问了
```

#### 步骤3：后续访问（直接访问）

```c
printf("%s", p);  // 后续读取
```

**此时发生了什么？**

```
1. CPU 访问虚拟地址 0x40000000
2. MMU 查页表，找到物理地址
3. 直接访问物理内存（页缓存），无需系统调用！
```

### 3.4 页缓存（Page Cache）机制

这是 mmap 高效的关键：

```
磁盘文件: mmap.txt
├─ 页0 (0-4095字节)    → 页缓存中的物理页A
├─ 页1 (4096-8191字节) → 页缓存中的物理页B
└─ ...

进程A的虚拟地址 0x40000000 → 页表 → 物理页A
进程B的虚拟地址 0x40000000 → 页表 → 物理页A  ← 共享同一块物理内存！
```

**关键点：**
- 文件数据被缓存在物理内存的页缓存中
- 多个进程的 mmap 可以指向同一块页缓存
- 修改会直接反映在页缓存中，其他进程可见

### 3.5 写回磁盘的机制

#### 数据何时写回磁盘？

```
1. 进程调用 msync() 强制同步
2. 系统定期刷新（由内核后台线程）
3. 内存压力大时，脏页被写回
4. 文件关闭或进程退出时
```

#### 写回过程：

```
进程写入: p[0] = 'H'
    ↓
页缓存中的物理页被标记为"脏"（dirty）
    ↓
内核后台线程定期检查
    ↓
将脏页写回磁盘文件
```

---

## 4. MAP_SHARED vs MAP_PRIVATE

### 4.1 核心区别

| 特性 | MAP_SHARED | MAP_PRIVATE |
|------|------------|-------------|
| **修改是否写回文件** | ✅ 是 | ❌ 否 |
| **其他进程是否可见修改** | ✅ 是 | ❌ 否 |
| **写时复制（COW）** | ❌ 否 | ✅ 是 |
| **内存使用** | 共享物理页 | 写入时复制 |
| **适用场景** | 进程间通信 | 只读或独立修改 |
| **性能** | 更快（无复制） | 首次写入时稍慢（需要复制） |

### 4.2 MAP_SHARED - 共享映射

#### 特点：
- ✅ 修改会写回文件
- ✅ 多个进程的修改互相可见
- ✅ 适合进程间通信

#### 内存布局：

```
进程A: 虚拟地址 0x40000000
    ↓ 页表
    └─→ 物理页X (页缓存) ←─┐
                           │ 共享同一块物理内存
进程B: 虚拟地址 0x40000000 │
    ↓ 页表                 │
    └─→ 物理页X (页缓存) ←─┘
                           │
                    ┌──────┘
                    ↓
                磁盘文件
```

#### 示例代码：

```c
// 进程A
int fd = open("shared.txt", O_CREAT | O_RDWR, 0644);
ftruncate(fd, 4096);
char *p1 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                MAP_SHARED, fd, 0);
strcpy(p1, "Hello from A");
// 数据立即写入页缓存，其他进程可见

// 进程B（同时运行）
int fd2 = open("shared.txt", O_RDWR);
char *p2 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                MAP_SHARED, fd2, 0);
printf("%s\n", p2);  // 输出: "Hello from A"（能看到进程A的修改）
```

### 4.3 MAP_PRIVATE - 私有映射（写时复制）

#### 特点：
- ❌ 修改不会写回文件
- ❌ 每个进程有独立的副本
- ✅ 使用写时复制（Copy-On-Write）

#### 内存布局（初始状态）：

```
进程A: 虚拟地址 0x40000000
    ↓ 页表
    └─→ 物理页X (页缓存) ←─┐
                           │ 初始时共享
进程B: 虚拟地址 0x40000000 │
    ↓ 页表                 │
    └─→ 物理页X (页缓存) ←─┘
```

#### 当进程A写入时（写时复制）：

```
进程A写入: strcpy(p1, "Hello")
    ↓
触发写时复制（COW）
    ↓
进程A: 虚拟地址 0x40000000
    ↓ 页表
    └─→ 物理页Y (新分配的页，包含修改)  ← 独立副本

进程B: 虚拟地址 0x40000000
    ↓ 页表
    └─→ 物理页X (页缓存，原始数据)     ← 仍然指向原始页
```

#### 示例代码：

```c
// 进程A
int fd = open("private.txt", O_CREAT | O_RDWR, 0644);
ftruncate(fd, 4096);
char *p1 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                MAP_PRIVATE, fd, 0);
strcpy(p1, "Hello from A");
// 数据写入进程A的私有副本，不会影响文件和其他进程

// 进程B（同时运行）
int fd2 = open("private.txt", O_RDWR);
char *p2 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                MAP_PRIVATE, fd2, 0);
printf("%s\n", p2);  // 输出: ""（空，看不到进程A的修改）
```

### 4.4 使用场景建议

#### 使用 MAP_SHARED 当：
- ✅ 需要进程间通信
- ✅ 需要将修改写回文件
- ✅ 多个进程需要看到彼此的修改
- ✅ 实现共享内存

```c
// 示例：共享配置
struct Config *config = mmap(..., MAP_SHARED, ...);
// 一个进程修改配置，其他进程立即看到
```

#### 使用 MAP_PRIVATE 当：
- ✅ 只读映射（性能优化）
- ✅ 每个进程需要独立修改
- ✅ 不想修改原文件
- ✅ 实现进程的私有内存

```c
// 示例：只读映射可执行文件
// 多个进程可以共享代码段，但各自有独立的数据段
```

---

## 5. 写时复制（COW）机制详解

### 5.1 MAP_PRIVATE 的工作流程

#### 初始状态：
```
进程A和B都映射到物理页X（共享）
```

#### 进程A读取：
```
└─→ 直接读取物理页X（无开销）
```

#### 进程A写入：
```
1. 触发写保护异常（页表标记为只读）
2. 内核分配新的物理页Y
3. 将物理页X的内容复制到物理页Y
4. 修改物理页Y
5. 更新进程A的页表：指向物理页Y
6. 进程B的页表仍然指向物理页X

结果：
- 进程A看到自己的修改
- 进程B看到原始数据
- 文件内容不变
```

### 5.2 详细过程分析

#### 阶段1：初始状态（fork 后，未写入）

```c
// 父进程
int fd = open("file.txt", O_RDWR);
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_PRIVATE, fd, 0);

pid_t pid = fork();  // 创建子进程
```

**内存布局：**

```
父进程：
虚拟地址: p (0x7f8a5c000000)
    ↓ 页表项
    ├─ 物理页号: 0x12345
    ├─ 权限: 只读（虽然 PROT_WRITE，但 MAP_PRIVATE 会标记为只读）
    └─ 标志: 存在、私有、可写（但页表项标记为只读）

子进程：
虚拟地址: p (0x7f8a5c000000)  ← 相同的虚拟地址
    ↓ 页表项
    ├─ 物理页号: 0x12345      ← 相同的物理页！
    ├─ 权限: 只读
    └─ 标志: 存在、私有、可写（但页表项标记为只读）

物理内存：
物理页 0x12345 (页缓存，包含文件内容)
    ↑
    └─ 被两个进程共享（引用计数 = 2）
```

**关键点：**
- 虚拟地址相同（都是 `p`）
- 物理页相同（都是 0x12345）
- 页表项标记为只读（用于触发 COW）

#### 阶段2：父进程写入（触发 COW）

```c
// 父进程写入
strcpy(p, "Hello from parent");
```

**详细过程：**

```
1. CPU 执行写入指令：mov [p], "Hello"
   ↓
2. MMU 查页表，发现页表项标记为"只读"
   ↓
3. 触发页面错误（Page Fault）
   ↓
4. 内核处理页面错误：
   a. 检查引用计数：发现物理页 0x12345 被多个进程共享（计数 > 1）
   b. 分配新的物理页：0x67890
   c. 复制数据：将物理页 0x12345 的内容复制到 0x67890
   d. 修改数据：在物理页 0x67890 上执行写入操作
   e. 更新父进程的页表：
      - 物理页号：0x12345 → 0x67890
      - 权限：只读 → 读写（现在可以正常写入了）
   f. 减少原物理页的引用计数：2 → 1
   ↓
5. 恢复用户程序执行，写入完成
```

**内存布局（写入后）：**

```
父进程：
虚拟地址: p (0x7f8a5c000000)
    ↓ 页表项
    ├─ 物理页号: 0x67890      ← 新的物理页！
    ├─ 权限: 读写
    └─ 标志: 存在、私有、可写

子进程：
虚拟地址: p (0x7f8a5c000000)  ← 仍然是相同的虚拟地址
    ↓ 页表项
    ├─ 物理页号: 0x12345      ← 仍然是原来的物理页！
    ├─ 权限: 只读
    └─ 标志: 存在、私有、可写（但页表项仍为只读）

物理内存：
物理页 0x12345 (原始数据，引用计数 = 1，只有子进程使用)
物理页 0x67890 (父进程的修改，引用计数 = 1)
```

#### 阶段3：子进程写入（再次触发 COW）

```c
// 子进程写入
strcpy(p, "Hello from child");
```

**详细过程：**

```
1. CPU 执行写入指令
   ↓
2. MMU 查页表，发现页表项标记为"只读"
   ↓
3. 触发页面错误
   ↓
4. 内核处理：
   a. 检查引用计数：物理页 0x12345 现在只有子进程使用（计数 = 1）
   b. 分配新的物理页：0xABCDE
   c. 复制数据：将物理页 0x12345 的内容复制到 0xABCDE
   d. 修改数据：在物理页 0xABCDE 上执行写入
   e. 更新子进程的页表：0x12345 → 0xABCDE
   f. 原物理页 0x12345 的引用计数变为 0，可以释放
   ↓
5. 恢复执行
```

**最终内存布局：**

```
父进程：
虚拟地址: p (0x7f8a5c000000)
    ↓ 页表项
    └─ 物理页号: 0x67890 (包含 "Hello from parent")

子进程：
虚拟地址: p (0x7f8a5c000000)  ← 相同的虚拟地址
    ↓ 页表项
    └─ 物理页号: 0xABCDE (包含 "Hello from child")

物理内存：
物理页 0x67890 (父进程的私有副本)
物理页 0xABCDE (子进程的私有副本)
物理页 0x12345 (已释放，引用计数 = 0)
```

### 5.3 关键点说明

#### 1. 为什么页表项标记为只读？

即使指定了 `PROT_READ | PROT_WRITE`，使用 `MAP_PRIVATE` 时，内核会将页表项标记为只读，用于：
- 检测首次写入
- 触发写时复制
- 实现私有映射

#### 2. 引用计数的作用

```
物理页引用计数：
- 初始：2（父进程 + 子进程）
- 父进程写入后：父进程页表指向新页，原页计数变为 1
- 子进程写入后：子进程页表指向新页，原页计数变为 0（可释放）
```

#### 3. 虚拟地址相同，物理地址不同

```
虚拟地址：相同（都是 p，比如 0x7f8a5c000000）
    ↓
页表转换
    ↓
物理地址：不同（父进程：0x67890，子进程：0xABCDE）
```

---

## 6. 映射区是否相同？

### 6.1 需要区分两个概念

#### 虚拟地址（Virtual Address）
- 每个进程的虚拟地址空间独立
- 不同进程映射同一文件，虚拟地址通常不同

#### 物理地址（Physical Address / 页缓存）
- 实际存储数据的物理内存
- 这是"映射区是否相同"的关键

### 6.2 MAP_SHARED 的情况

#### 虚拟地址：通常不同

```c
// 进程A
char *p1 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                MAP_SHARED, fd, 0);
// p1 = 0x7f8a5c000000（示例）

// 进程B（独立进程）
char *p2 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                MAP_SHARED, fd, 0);
// p2 = 0x7f9b6d100000（示例，通常不同）
```

#### 物理地址：相同（共享页缓存）

```
进程A：
虚拟地址: 0x7f8a5c000000
    ↓ 页表
    └─→ 物理页 0x12345 (页缓存) ←─┐
                                  │ 共享同一块物理内存
进程B：                            │
虚拟地址: 0x7f9b6d100000          │
    ↓ 页表                        │
    └─→ 物理页 0x12345 (页缓存) ←─┘
                                  │
                           ┌──────┘
                           ↓
                    磁盘文件: mmap.txt
```

#### 写入后的行为：

```c
// 进程A写入
strcpy(p1, "Hello from A");
// 直接修改物理页 0x12345

// 进程B读取
printf("%s\n", p2);  // 输出: "Hello from A"
// 读取的是同一个物理页 0x12345
```

**结论（MAP_SHARED）：**
- ✅ 虚拟地址：通常不同
- ✅ 物理地址：相同（共享同一块页缓存）
- ✅ 修改会立即反映到所有映射的进程

### 6.3 MAP_PRIVATE 的情况

#### 初始状态：虚拟地址不同，物理地址相同

```c
// 进程A
char *p1 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                MAP_PRIVATE, fd, 0);
// p1 = 0x7f8a5c000000

// 进程B
char *p2 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                MAP_PRIVATE, fd, 0);
// p2 = 0x7f9b6d100000（虚拟地址不同）
```

**初始内存布局：**

```
进程A：
虚拟地址: 0x7f8a5c000000
    ↓ 页表（只读标记）
    └─→ 物理页 0x12345 (页缓存，引用计数=2) ←─┐
                                              │ 初始时共享
进程B：                                        │
虚拟地址: 0x7f9b6d100000                      │
    ↓ 页表（只读标记）                        │
    └─→ 物理页 0x12345 (页缓存，引用计数=2) ←─┘
```

#### 写入后：触发写时复制（COW），物理地址变为不同

```c
// 进程A写入
strcpy(p1, "Hello from A");
// 触发 COW：
// 1. 分配新物理页 0x67890
// 2. 复制数据：0x12345 → 0x67890
// 3. 修改新页
// 4. 更新进程A的页表：0x12345 → 0x67890
```

**写入后的内存布局：**

```
进程A：
虚拟地址: 0x7f8a5c000000
    ↓ 页表（读写）
    └─→ 物理页 0x67890 (私有副本，包含 "Hello from A")

进程B：
虚拟地址: 0x7f9b6d100000
    ↓ 页表（只读）
    └─→ 物理页 0x12345 (原始数据，引用计数=1)
```

**结论（MAP_PRIVATE）：**
- ✅ 虚拟地址：通常不同
- ✅ 物理地址：
  - 初始：相同（共享页缓存）
  - 写入后：不同（每个进程有自己的副本）

### 6.4 总结对比表

| 特性 | MAP_SHARED | MAP_PRIVATE |
|------|------------|-------------|
| 虚拟地址 | 通常不同 | 通常不同 |
| 初始物理地址 | 相同（共享页缓存） | 相同（共享页缓存） |
| 写入后物理地址 | 相同（继续共享） | 不同（COW 后独立） |
| 修改是否可见 | 是（所有进程可见） | 否（各自独立） |
| 文件是否更新 | 是（写回文件） | 否（不写回文件） |

---

## 7. 父子进程通信

### 7.1 基本流程

```c
// 父进程
int fd = open("file.txt", O_CREAT | O_RDWR, 0644);
ftruncate(fd, 4096);
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 0);

pid_t pid = fork();  // 创建子进程

if (pid == 0) {
    // 子进程：写入数据
    strcpy(p, "Hello from child");
} else {
    // 父进程：读取数据
    sleep(1);
    printf("%s\n", p);  // 输出: "Hello from child"
    wait(NULL);
}
```

### 7.2 为什么可以通信？

- `fork()` 会继承文件描述符
- 父子进程使用同一个 `fd`，映射到相同的物理内存
- 因此可以通信

### 7.3 内存布局

```
父进程：
fd = 3 → file.txt
    ↓ mmap
虚拟地址: 0x7f8a5c000000
    ↓ 页表
    └─→ 物理页 0x12345 (页缓存) ←─┐
                                 │ 共享同一块物理内存
子进程（fork后）：                │
fd = 3 → file.txt (继承)         │
    ↓ mmap                      │
虚拟地址: 0x7f8a5c000000        │
    ↓ 页表                      │
    └─→ 物理页 0x12345 (页缓存) ←─┘
```

### 7.4 完整示例

```c
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main() {
    int fd = open("shared.txt", O_CREAT | O_TRUNC | O_RDWR, 0644);
    ftruncate(fd, 4096);
    
    char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                   MAP_SHARED, fd, 0);
    
    pid_t pid = fork();
    
    if (pid == 0) {
        // 子进程：写入数据
        strcpy(p, "Hello from child");
        printf("子进程写入: %s\n", p);
    } else {
        // 父进程：等待并读取
        sleep(1);
        printf("父进程读取: %s\n", p);
        
        // 父进程也可以修改
        strcat(p, " Modified by parent!");
        printf("父进程修改后: %s\n", p);
        
        wait(NULL);
        munmap(p, 4096);
        close(fd);
    }
    
    return 0;
}
```

---

## 8. 无血缘关系进程通信

### 8.1 基本流程

```c
// 进程A
int fd1 = open("/tmp/shared.txt", O_CREAT | O_RDWR, 0644);
ftruncate(fd1, 4096);
char *p1 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                MAP_SHARED, fd1, 0);
strcpy(p1, "Hello from A");

// 进程B（完全独立的进程）
int fd2 = open("/tmp/shared.txt", O_RDWR);
char *p2 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                MAP_SHARED, fd2, 0);
printf("%s\n", p2);  // 输出: "Hello from A"（能看到进程A的修改）
```

### 8.2 为什么可以通信？

- 所有进程打开同一个文件
- 使用 `MAP_SHARED` 标志
- 映射到相同的文件区域
- 虚拟地址可能不同，但物理页是共享的（通过页缓存）

### 8.3 内存布局

```
进程A（独立进程）：
虚拟地址: 0x7f8a5c000000
    ↓ 页表
    └─→ 物理页X (页缓存) ←─┐
                           │ 共享同一块物理内存
进程B（独立进程）：
虚拟地址: 0x7f9b6d100000  │  ← 虚拟地址不同！
    ↓ 页表                 │
    └─→ 物理页X (页缓存) ←─┘
                           │
                    ┌──────┘
                    ↓
                磁盘文件: shared.txt
```

### 8.4 完整示例

#### 写入进程（writer.c）

```c
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define FILESIZE 1024

int main() {
    int fd = open("/tmp/shared_mmap.txt", O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (fd == -1) {
        perror("open error");
        exit(1);
    }
    
    ftruncate(fd, FILESIZE);
    
    char *p = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, 
                   MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap error");
        close(fd);
        exit(1);
    }
    
    printf("写入进程：虚拟地址 = %p\n", p);
    
    int i = 0;
    while (1) {
        sprintf(p, "Message %d from writer", i++);
        printf("写入进程发送: %s\n", p);
        sleep(2);
        
        if (strncmp(p, "quit", 4) == 0) {
            break;
        }
    }
    
    munmap(p, FILESIZE);
    close(fd);
    return 0;
}
```

#### 读取进程（reader.c）

```c
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define FILESIZE 1024

int main() {
    int fd = open("/tmp/shared_mmap.txt", O_RDWR);
    if (fd == -1) {
        perror("open error");
        exit(1);
    }
    
    char *p = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, 
                   MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap error");
        close(fd);
        exit(1);
    }
    
    printf("读取进程：虚拟地址 = %p\n", p);
    // 注意：虚拟地址可能不同，但物理页是共享的！
    
    char last_msg[FILESIZE] = "";
    while (1) {
        if (strcmp(p, last_msg) != 0) {
            printf("收到数据：%s\n", p);
            strcpy(last_msg, p);
            
            if (strstr(p, "Message 5") != NULL) {
                printf("检测到 Message 5，发送退出信号\n");
                strcpy(p, "quit");
                break;
            }
        }
        sleep(1);
    }
    
    munmap(p, FILESIZE);
    close(fd);
    return 0;
}
```

### 8.4 关键点

1. **文件路径要一致**：所有进程必须使用相同的文件路径
2. **文件大小要足够**：确保文件大小 >= 映射大小
3. **需要同步机制（可选）**：如果多个进程同时写入，可能需要加锁

---

## 9. /dev/zero 与匿名映射

### 9.1 /dev/zero 是什么？

`/dev/zero` 是一个特殊的设备文件（字符设备），不是普通文件。

#### 特点：

```c
// /dev/zero 的特性：
1. 读取时：返回无限个 '\0'（零字节）
2. 写入时：数据被丢弃（写入黑洞）
3. 大小：虚拟无限大（不需要 ftruncate）
4. 用途：提供零初始化的内存映射
```

#### 为什么不需要 `ftruncate`？

```c
// 普通文件
int fd = open("file.txt", O_RDWR);
ftruncate(fd, 4096);  // 必须！因为文件初始大小为 0
char *p = mmap(..., fd, 0);

// /dev/zero
int fd = open("/dev/zero", O_RDWR);
// 不需要 ftruncate！因为 /dev/zero 是"虚拟无限大"
char *p = mmap(..., fd, 0);
```

**原因：**
- `/dev/zero` 是设备文件，不是普通文件
- 它提供"虚拟无限大"的零字节流
- 内核会按需分配物理内存，不需要预先设置文件大小

### 9.2 匿名映射（MAP_ANONYMOUS）

#### 什么是匿名映射？

匿名映射不关联任何文件，直接在内存中分配空间。

```c
// 匿名映射（不需要文件）
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
//                                              ↑
//                                         fd = -1（不使用文件）
```

### 9.3 /dev/zero vs 匿名映射对比

| 特性 | `/dev/zero` | `MAP_ANONYMOUS` |
|------|-------------|-----------------|
| 需要文件 | ✅ 需要打开 `/dev/zero` | ❌ 不需要文件 |
| 需要 ftruncate | ❌ 不需要 | ❌ 不需要 |
| 初始化 | 零初始化 | 零初始化 |
| 性能 | 稍慢（需要设备文件） | **更快（直接分配）** |
| 可移植性 | 较好 | **更好（POSIX 标准）** |
| 推荐度 | 较旧方式 | **✅ 现代推荐** |

### 9.4 /dev/zero 能否支持无血缘关系进程通信？

**结论：不能！** `/dev/zero` 不能用于无血缘关系进程间的通信。

#### 为什么父子进程可以通信？

```c
// 父进程
int fd = open("/dev/zero", O_RDWR);
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 0);

pid_t pid = fork();  // 子进程继承文件描述符 fd

// 父子进程共享同一个文件描述符，映射到相同的物理内存
```

#### 为什么无血缘关系进程不能通信？

```c
// 进程A
int fd1 = open("/dev/zero", O_RDWR);
char *p1 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                MAP_SHARED, fd1, 0);
*p1 = 100;

// 进程B（完全独立的进程）
int fd2 = open("/dev/zero", O_RDWR);  // 新的文件描述符！
char *p2 = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                MAP_SHARED, fd2, 0);
printf("%d\n", *p2);  // 输出: 0（看不到进程A的修改！）
```

**问题：**
- 每个进程打开 `/dev/zero` 得到不同的文件描述符
- 设备文件的映射是独立的，不共享物理内存
- 进程A的修改不会反映到进程B

#### 内存布局对比

**父子进程（可以通信）：**

```
父进程：
fd = 3 → /dev/zero
    ↓ mmap
虚拟地址: 0x7f8a5c000000
    ↓ 页表
    └─→ 物理页 0x12345 ←─┐
                        │ 共享同一块物理内存
子进程（fork后）：       │
fd = 3 → /dev/zero (继承)│
    ↓ mmap              │
虚拟地址: 0x7f8a5c000000 │
    ↓ 页表              │
    └─→ 物理页 0x12345 ←─┘
```

**无血缘关系进程（不能通信）：**

```
进程A：
fd1 = 3 → /dev/zero
    ↓ mmap
虚拟地址: 0x7f8a5c000000
    ↓ 页表
    └─→ 物理页 0x12345 (独立的内存)

进程B：
fd2 = 3 → /dev/zero (新的文件描述符！)
    ↓ mmap
虚拟地址: 0x7f9b6d100000
    ↓ 页表
    └─→ 物理页 0x67890 (独立的内存，不同！)
```

### 9.5 为什么普通文件可以，/dev/zero 不行？

#### 普通文件（可以通信）：

```c
// 进程A
int fd1 = open("shared.txt", O_RDWR);
ftruncate(fd1, 4096);
char* p1 = mmap(..., MAP_SHARED, fd1, 0);
strcpy(p1, "Hello");

// 进程B
int fd2 = open("shared.txt", O_RDWR);  // 打开同一个文件
char* p2 = mmap(..., MAP_SHARED, fd2, 0);
printf("%s\n", p2);  // 输出: "Hello"（可以看到！）
```

**原因：**
- 普通文件通过页缓存（Page Cache）共享
- 多个进程映射同一文件时，映射到相同的页缓存
- 修改会反映到所有映射的进程

#### /dev/zero（不能通信）：

```c
// 进程A
int fd1 = open("/dev/zero", O_RDWR);
char* p1 = mmap(..., MAP_SHARED, fd1, 0);
strcpy(p1, "Hello");

// 进程B
int fd2 = open("/dev/zero", O_RDWR);  // 设备文件，每次打开都是独立的
char* p2 = mmap(..., MAP_SHARED, fd2, 0);
printf("%s\n", p2);  // 输出: ""（看不到！）
```

**原因：**
- `/dev/zero` 是设备文件，不是普通文件
- 设备文件的映射不通过页缓存
- 每次映射都分配独立的物理内存
- 不同进程的映射互不共享

### 9.6 总结对比表

| 方式 | 父子进程 | 无血缘关系进程 | 原因 |
|------|---------|---------------|------|
| `/dev/zero` + MAP_SHARED | ✅ 可以 | ❌ 不可以 | 设备文件映射不共享 |
| 普通文件 + MAP_SHARED | ✅ 可以 | ✅ 可以 | 通过页缓存共享 |
| MAP_ANONYMOUS + MAP_SHARED | ✅ 可以 | ❌ 不可以 | 需要继承映射关系 |

---

## 10. 使用注意事项

### 10.1 文件大小问题

#### 问题1：文件大小为 0，但映射长度非 0

```c
int fd = open("file.txt", O_CREAT | O_RDWR, 0644);
// 此时文件大小为 0

char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 0);
// 错误：尝试映射 4096 字节，但文件只有 0 字节
// 结果：Bus Error
```

**解决方法：**

```c
int fd = open("file.txt", O_CREAT | O_RDWR, 0644);
ftruncate(fd, 4096);  // 先扩展文件大小
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 0);
```

#### 问题2：文件大小为 0，映射长度也为 0

```c
int fd = open("file.txt", O_CREAT | O_RDWR, 0644);
char *p = mmap(NULL, 0, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 0);
// 错误：Invalid Parameter
// 映射长度为 0 没有意义
```

### 10.2 权限问题

#### 问题3：文件只读，但映射要求读写

```c
int fd = open("file.txt", O_RDONLY);  // 只读打开
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 0);
// 错误：Invalid Parameter
// mmap 的权限不能超过文件打开权限
```

**解决方法：**

```c
int fd = open("file.txt", O_RDWR);  // 读写打开
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 0);
```

#### 问题4：MAP_SHARED 需要读权限

```c
int fd = open("file.txt", O_WRONLY);  // 只写打开
char *p = mmap(NULL, 4096, PROT_WRITE, 
               MAP_SHARED, fd, 0);
// 错误：Permission denied
// MAP_SHARED 时，即使只写，也需要读权限
```

**解决方法：**

```c
int fd = open("file.txt", O_RDWR);  // 读写打开
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 0);
```

#### 问题5：MAP_PRIVATE 只需要读权限

```c
// MAP_PRIVATE 时，即使指定 PROT_WRITE，文件只需要读权限
int fd = open("file.txt", O_RDONLY);  // 只读打开即可
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_PRIVATE, fd, 0);
// 可以！因为写时复制机制，不会修改原文件
```

### 10.3 偏移量问题

#### 问题6：offset 必须是页大小的整数倍

```c
int fd = open("file.txt", O_RDWR);
ftruncate(fd, 8192);

char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 2);  // offset = 2
// 错误：Invalid argument
// offset 必须是 4096 的整数倍（页大小）
```

**解决方法：**

```c
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 0);  // offset = 0（4096 的倍数）

// 或者
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 4096);  // offset = 4096（4096 的倍数）
```

### 10.4 文件描述符问题

#### 问题7：映射成功后可以关闭文件描述符

```c
int fd = open("file.txt", O_RDWR);
ftruncate(fd, 4096);
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 0);

close(fd);  // 可以立即关闭！
// 映射成功后，后续操作通过 p 进行，不需要 fd

strcpy(p, "Hello");  // 正常使用
```

### 10.5 内存访问问题

#### 问题8：不能越界访问

```c
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 0);

p[4096] = 'X';  // 错误：越界访问，会导致段错误
```

### 10.6 munmap 问题

#### 问题9：munmap 地址必须精确

```c
char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_SHARED, fd, 0);

// 错误：不能使用偏移后的地址
char *ptr = p + 100;
munmap(ptr, 4096);  // 错误！

// 正确：必须使用原始地址
munmap(p, 4096);  // 正确
```

### 10.7 MAP_PRIVATE 行为

#### 问题10：MAP_PRIVATE 修改不写回文件

```c
int fd = open("file.txt", O_RDWR);
ftruncate(fd, 4096);
write(fd, "Original", 8);

char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
               MAP_PRIVATE, fd, 0);

strcpy(p, "Modified");  // 修改映射区

// 检查文件内容
lseek(fd, 0, SEEK_SET);
char buf[100];
read(fd, buf, sizeof(buf));
printf("%s\n", buf);  // 输出: "Original"（文件未被修改）
```

---

## 11. 保险调用方式

### 11.1 标准调用流程

```c
// 步骤1：打开文件（读写模式）
int fd = open("文件名", O_RDWR);
if (fd == -1) {
    perror("open error");
    exit(1);
}

// 步骤2：扩展文件大小（如果需要）
ftruncate(fd, 4096);  // 确保文件大小 >= 映射大小

// 步骤3：映射文件
char *p = mmap(NULL,                // 让系统选择地址
               4096,                 // 映射大小
               PROT_READ | PROT_WRITE,  // 读写权限
               MAP_SHARED,           // 共享映射
               fd,                   // 文件描述符
               0);                    // 从文件开头映射

if (p == MAP_FAILED) {
    perror("mmap error");
    close(fd);
    exit(1);
}

// 步骤4：可以关闭文件描述符（可选）
close(fd);

// 步骤5：使用映射的内存
strcpy(p, "Hello mmap");

// 步骤6：取消映射
munmap(p, 4096);
```

### 11.2 完整示例

```c
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

void sys_err(const char* str){
    perror(str);
    exit(1);
}

int main(int argc, char* argv[]){
    char *p = NULL;
    int fd;
    
    // 1. 打开文件（创建并清空，读写模式）
    fd = open("mmap.txt", O_CREAT | O_TRUNC | O_RDWR, 0644);
    if(fd == -1){
        sys_err("open error");
    }
    
    // 2. 扩展文件大小（mmap 需要文件有足够大小）
    ftruncate(fd, 4096);  // 扩展到 4096 字节
    
    // 3. 映射文件到内存
    p = mmap(NULL,                    // 让系统选择地址
             4096,                    // 映射 4096 字节
             PROT_READ | PROT_WRITE,  // 读写权限
             MAP_SHARED,              // 共享映射（用于进程间通信）
             fd,                      // 文件描述符
             0);                      // 从文件开头映射
    
    if (p == MAP_FAILED) {
        sys_err("mmap error");
    }
    
    // 4. 使用映射的内存
    strcpy(p, "Hello, mmap!");
    printf("写入的内容: %s\n", p);
    
    // 5. 取消映射
    munmap(p, 4096);
    close(fd);
    
    return 0;
}
```

### 11.3 错误检查清单

- [ ] 文件打开是否成功？
- [ ] 文件大小是否足够（使用 `ftruncate`）？
- [ ] 文件权限是否匹配（`O_RDWR` 对应 `PROT_READ | PROT_WRITE`）？
- [ ] `offset` 是否是 4096 的整数倍？
- [ ] `mmap` 返回值是否检查（`MAP_FAILED`）？
- [ ] 访问内存时是否越界？
- [ ] `munmap` 是否使用原始地址？

---

## 12. 实际应用示例

### 12.1 父子进程通信（MAP_SHARED）

```c
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main() {
    int fd = open("shared.txt", O_CREAT | O_TRUNC | O_RDWR, 0644);
    ftruncate(fd, 4096);
    
    char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                   MAP_SHARED, fd, 0);
    
    pid_t pid = fork();
    
    if (pid == 0) {
        // 子进程：写入数据
        strcpy(p, "Hello from child");
        printf("子进程写入: %s\n", p);
    } else {
        // 父进程：等待并读取
        sleep(1);
        printf("父进程读取: %s\n", p);
        
        wait(NULL);
        munmap(p, 4096);
        close(fd);
    }
    
    return 0;
}
```

### 12.2 无血缘关系进程通信

#### 写入进程

```c
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define FILESIZE 1024

int main() {
    int fd = open("/tmp/shared_mmap.txt", O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (fd == -1) {
        perror("open error");
        exit(1);
    }
    
    ftruncate(fd, FILESIZE);
    
    char *p = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, 
                   MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap error");
        close(fd);
        exit(1);
    }
    
    int i = 0;
    while (1) {
        sprintf(p, "Message %d from writer", i++);
        printf("写入: %s\n", p);
        sleep(2);
        
        if (strncmp(p, "quit", 4) == 0) {
            break;
        }
    }
    
    munmap(p, FILESIZE);
    close(fd);
    return 0;
}
```

#### 读取进程

```c
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define FILESIZE 1024

int main() {
    int fd = open("/tmp/shared_mmap.txt", O_RDWR);
    if (fd == -1) {
        perror("open error");
        exit(1);
    }
    
    char *p = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, 
                   MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap error");
        close(fd);
        exit(1);
    }
    
    char last_msg[FILESIZE] = "";
    while (1) {
        if (strcmp(p, last_msg) != 0) {
            printf("收到: %s\n", p);
            strcpy(last_msg, p);
            
            if (strstr(p, "Message 5") != NULL) {
                strcpy(p, "quit");
                break;
            }
        }
        sleep(1);
    }
    
    munmap(p, FILESIZE);
    close(fd);
    return 0;
}
```

### 12.3 匿名映射（父子进程）

```c
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main() {
    // 匿名映射（不需要文件）
    char *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, 
                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    if (p == MAP_FAILED) {
        perror("mmap error");
        exit(1);
    }
    
    strcpy(p, "Hello MMAP_ANONYMOUS!!");
    printf("父进程：虚拟地址 = %p, 内容 = %s\n", p, p);
    
    pid_t pid = fork();
    
    if (pid == 0) {
        // 子进程
        printf("子进程：虚拟地址 = %p, 内容 = %s\n", p, p);
        strcpy(p, "Child modified");
        printf("子进程写入后：%s\n", p);
        sleep(2);
    } else {
        // 父进程
        sleep(1);
        printf("父进程读取：%s\n", p);
        strcpy(p, "Parent modified");
        printf("父进程写入后：%s\n", p);
        
        wait(NULL);
        munmap(p, 4096);
    }
    
    return 0;
}
```

---

## 总结

### 关键要点

1. **mmap 的优势**：零拷贝、高性能、随机访问、易用性
2. **映射原理**：建立虚拟地址与物理内存的对应关系，通过页缓存共享
3. **MAP_SHARED**：修改写回文件，进程间可见，适合进程间通信
4. **MAP_PRIVATE**：修改不写回文件，使用写时复制，适合独立修改
5. **父子进程通信**：可以继承文件描述符和映射关系
6. **无血缘关系进程通信**：必须使用普通文件 + MAP_SHARED，不能使用 /dev/zero 或匿名映射
7. **使用注意事项**：文件大小、权限匹配、偏移量对齐、越界检查等

### 选择建议

- **父子进程通信**：可以使用 `/dev/zero`、`MAP_ANONYMOUS` 或普通文件
- **无血缘关系进程通信**：必须使用普通文件 + `MAP_SHARED`
- **需要持久化**：使用普通文件
- **不需要持久化**：使用 `MAP_ANONYMOUS`（仅限父子进程）

---

*本笔记整理自实际学习和实践，涵盖了 mmap 的核心概念、原理、使用方法和注意事项。*
