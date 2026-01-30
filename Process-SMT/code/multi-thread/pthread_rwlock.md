# pthread 读写锁（Read-Write Lock）

## 1. 为什么需要读写锁

互斥锁（`pthread_mutex_t`）保证同一时刻只有一个线程访问共享资源。但在「读多写少」的场景下，多个线程**只读**数据时并不会互相修改，理论上可以并发执行；只有**写**操作才需要独占。若一律用互斥锁，会不必要地串行化所有读操作，降低并发度。

**读写锁**在语义上区分：

- **读锁（共享锁）**：多个线程可以同时持有读锁，用于只读访问。
- **写锁（独占锁）**：同一时刻只能有一个线程持有写锁，且持有写锁时不能有读锁或其他写锁。

这样，多读者可以并发，写者仍然独占，在「读多写少」时能提高吞吐。

---

## 2. 读写锁的规则（原理）

| 当前状态     | 再申请读锁     | 再申请写锁     |
|--------------|----------------|----------------|
| 无锁         | 允许           | 允许           |
| 已有读锁     | 允许（多读者） | **阻塞**       |
| 已有写锁     | **阻塞**       | **阻塞**       |

- 已有**读锁**时：新读者可以加读锁；写者必须等待所有读锁释放。
- 已有**写锁**时：新的读者和写者都要等待写锁释放。

常见实现策略（了解即可）：

- **读者优先**：有读者在等或正在读时，写者容易长时间等待。
- **写者优先**：有写者在等时，新到的读者要等该写者完成，避免写者饿死。
- **公平策略**：按到达顺序排队，避免某一方长期饿死。

Linux 下 `pthread_rwlock_t` 的具体策略以 man 手册和实现为准。

---

## 3. 相关数据类型

```c
#include <pthread.h>

pthread_rwlock_t rwlock;        // 读写锁对象
pthread_rwlockattr_t attr;       // 可选：读写锁属性
```

---

## 4. 常用 API

### 4.1 初始化与销毁

```c
int pthread_rwlock_init(pthread_rwlock_t *restrict rwlock,
                        const pthread_rwlockattr_t *restrict attr);
```

- **作用**：对 `rwlock` 做动态初始化。
- **attr**：通常传 `NULL`，使用默认属性。
- **返回值**：成功返回 `0`，失败返回错误码（非 errno）。

```c
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
```

- **作用**：销毁读写锁，释放内部资源。
- **要求**：调用时锁必须处于「未锁定」状态，且没有线程在等待该锁。

---

### 4.2 读锁（共享锁）

```c
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
```

- **作用**：加读锁；若已有写锁或正在等待的写锁（视实现而定），则可能阻塞。
- **返回值**：成功返回 `0`，失败返回错误码。

```c
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
```

- **作用**：尝试加读锁，**不阻塞**。
- **返回值**：成功返回 `0`；若无法立即获得锁则返回 `EBUSY` 等错误码。

---

### 4.3 写锁（独占锁）

```c
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
```

- **作用**：加写锁；若已有读锁或写锁，则阻塞。
- **返回值**：成功返回 `0`，失败返回错误码。

```c
int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);
```

- **作用**：尝试加写锁，**不阻塞**。
- **返回值**：成功返回 `0`；无法立即获得锁时返回 `EBUSY` 等。

---

### 4.4 解锁

```c
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);
```

- **作用**：释放当前线程持有的读锁或写锁（同一把锁用同一个 `unlock`，无需区分读/写）。
- **返回值**：成功返回 `0`，失败返回错误码。

---

### 4.5 带超时的加锁（可选）

```c
int pthread_rwlock_timedrdlock(pthread_rwlock_t *restrict rwlock,
                               const struct timespec *restrict abstime);
int pthread_rwlock_timedwrlock(pthread_rwlock_t *restrict rwlock,
                               const struct timespec *restrict abstime);
```

- **作用**：在 `abstime` 指定的绝对时间之前尝试加读锁/写锁，超时则返回 `ETIMEDOUT`。
- **注意**：需要 `_POSIX_TIMEOUTS` 或相应特性，部分环境可能未提供。

---

## 5. 使用要点

1. **谁加锁谁解锁**：同一线程对同一把 `pthread_rwlock_t` 加锁后，必须在线程内配对调用 `pthread_rwlock_unlock`，否则会死锁或未定义行为。
2. **读锁内不要写**：加读锁时约定只读共享数据；若在读锁内写，会破坏「多读者可并发」的语义，且可能产生数据竞争。
3. **写锁内可读可写**：写锁独占，在写锁内读、写都是安全的。
4. **错误检查**：`pthread_rwlock_*` 返回的是 errno 风格错误码（如 `EAGAIN`、`EBUSY`、`EDEADLK`），应检查返回值并做相应处理，尤其在用 `try*` 和 `timed*` 时。

---

## 6. 与互斥锁的对比

| 特性           | 互斥锁 `pthread_mutex_t` | 读写锁 `pthread_rwlock_t` |
|----------------|--------------------------|----------------------------|
| 读-读          | 互斥，不能并发           | 可并发（多读者）           |
| 读-写 / 写-写  | 互斥                     | 互斥                       |
| 适用场景       | 任意临界区               | 读多写少、读可并发         |
| 实现开销       | 一般更小                 | 通常略大                   |

在「读多写少」且读操作占主导时，读写锁能明显提高并发度；若临界区很短或写很多，互斥锁往往更简单、开销更可控。

---

## 7. 示例代码说明

同目录下的 `rwlock-demo.c` 演示了：

- 使用 `pthread_rwlock_init` / `pthread_rwlock_destroy` 管理读写锁生命周期。
- **读者线程**：循环内调用 `pthread_rwlock_rdlock` → 读共享缓冲区 → `pthread_rwlock_unlock`，多个读者可同时执行。
- **写者线程**：循环内调用 `pthread_rwlock_wrlock` → 更新共享缓冲区和版本号 → `pthread_rwlock_unlock`，写时独占。
- 主线程创建多读者、多写者并 `pthread_join`，最后销毁锁并打印最终状态。

编译与运行：

```bash
gcc -o rwlock-demo rwlock-demo.c -lpthread
./rwlock-demo
```

通过输出可以看到：读者会读到不同版本的内容，写者会互斥地更新版本与缓冲区，从而理解「读共享、写独占」的行为。
