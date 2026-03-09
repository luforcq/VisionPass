#include "at24c64.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <stdio.h>

AT24C64::AT24C64() : fd(-1)
{
}

AT24C64::~AT24C64()
{
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

// 初始化I2C
bool AT24C64::init()
{
    fd = open(I2C_DEV, O_RDWR);
    if (fd < 0) {
        perror("ERROR: 打开I2C设备失败");
        return false;
    }

    // 设置从机地址
    if (ioctl(fd, I2C_SLAVE, AT24C64_I2C_ADDR) < 0) {
        perror("ERROR: 设置I2C地址失败");
        close(fd);
        fd = -1;
        return false;
    }
    printf("I2C初始化成功\n");
    return true;
}

// 读1字节
uint8_t AT24C64::read(uint16_t addr)
{
    uint8_t buf[2] = { (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF) };
    uint8_t data = 0xFF;

    if (fd < 0) return 0xFF;

    ::write(fd, buf, 2);
    ::read(fd, &data, 1);
    return data;
}

// 写1字节
bool AT24C64::write(uint16_t addr, uint8_t data)
{
    uint8_t buf[3] = {
        (uint8_t)(addr >> 8),
        (uint8_t)(addr & 0xFF),
        data
    };

    if (fd < 0) return false;

    if (::write(fd, buf, 3) != 3) {
        perror("ERROR: 写1字节失败");
        return false;
    }

    usleep(5000); // 写周期等待5ms
    return true;
}

// 连续读
bool AT24C64::read_buf(uint16_t addr, uint8_t *buf, int len)
{
    uint8_t addr_buf[2] = { (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF) };

    if (fd < 0 || !buf || len <= 0)
        return false;

    ::write(fd, addr_buf, 2);
    if (::read(fd, buf, len) != len) {
        perror("ERROR: 连续读失败");
        return false;
    }
    return true;
}

// 连续写（自动分页）
bool AT24C64::write_buf(uint16_t addr, const uint8_t *buf, int len)
{
    int remain = len;
    int pos = 0;

    if (fd < 0 || !buf || len <= 0)
        return false;

    while (remain > 0) {
        int off = addr % AT24C64_PAGE_SIZE;
        int wlen = AT24C64_PAGE_SIZE - off;
        if (wlen > remain) wlen = remain;

        uint8_t wbuf[34];
        wbuf[0] = (uint8_t)(addr >> 8);
        wbuf[1] = (uint8_t)(addr & 0xFF);
        memcpy(&wbuf[2], &buf[pos], wlen);

        if (::write(fd, wbuf, 2 + wlen) != 2 + wlen) {
            perror("ERROR: 连续写失败");
            return false;
        }

        usleep(5000); // 每页等待5ms

        addr += wlen;
        pos += wlen;
        remain -= wlen;
    }
    return true;
}

// ===================== 业务层函数 =====================
// 初始化EEPROM（首次使用）
bool AT24C64::initEEPROM() {
    EEP_Head head = {0};
    read_buf(0x0000, (uint8_t*)&head, sizeof(EEP_Head));

    if (head.magic != 0x5A5A) {
        qDebug("EEPROM未初始化，开始初始化...\n");
        head.magic = 0x5A5A;
        head.user_count = 0;
        if (!write_buf(0x0000, (uint8_t*)&head, sizeof(EEP_Head))) {
            qDebug("ERROR: EEPROM初始化失败\n");
            return false;
        }
        qDebug("EEPROM初始化成功\n");
    } else {
        qDebug("EEPROM已初始化\n");
    }
    return true;
}

// 写密钥
bool AT24C64::writeKey(EEP_Key *key) {
    if (!write_buf(0x0020, (uint8_t*)key, sizeof(EEP_Key))) {
        qDebug("ERROR: 写密钥失败\n");
        return false;
    }
    qDebug("写密钥成功\n");
    return true;
}

// 读密钥
bool AT24C64::readKey(EEP_Key *key) {
    if (!read_buf(0x0020, (uint8_t*)key, sizeof(EEP_Key))) {
        qDebug("ERROR: 读密钥失败\n");
        return false;
    }
    qDebug("读密钥成功\n");
    return true;
}

// 写密码
bool AT24C64::writePwd(EEP_Pwd *pwd) {
    if (!write_buf(0x0040, (uint8_t*)pwd, sizeof(EEP_Pwd))) {
        qDebug("ERROR: 写密码失败\n");
        return false;
    }
    qDebug("写密码成功\n");
    return true;
}

// 读密码
bool AT24C64::readPwd(EEP_Pwd *pwd) {
    if (!read_buf(0x0040, (uint8_t*)pwd, sizeof(EEP_Pwd))) {
        qDebug("ERROR: 读密码失败\n");
        return false;
    }
    qDebug("读密码成功\n");
    return true;
}

// 写参数
bool AT24C64::writeParam(EEP_Param *param) {
    if (!write_buf(0x0060, (uint8_t*)param, sizeof(EEP_Param))) {
        qDebug("ERROR: 写参数失败\n");
        return false;
    }
    qDebug("写参数成功\n");
    return true;
}

// 读参数
bool AT24C64::readParam(EEP_Param *param) {
    if (!read_buf(0x0060, (uint8_t*)param, sizeof(EEP_Param))) {
        qDebug("ERROR: 读参数失败\n");
        return false;
    }
    qDebug("读参数成功\n");
    return true;
}

// 新增用户
bool AT24C64::addUser(EEP_User *user) {
    EEP_Head head;
    read_buf(0x0000, (uint8_t*)&head, sizeof(EEP_Head));

    uint16_t addr = USER_BASE_ADDR + head.user_count * USER_SIZE;
    if (!write_buf(addr, (uint8_t*)user, sizeof(EEP_User))) {
        qDebug("ERROR: 新增用户失败\n");
        return false;
    }

    // 更新用户数
    head.user_count++;
    write_buf(0x0000, (uint8_t*)&head, sizeof(EEP_Head));
    qDebug("新增用户成功，当前用户数：%d\n", head.user_count);
    return true;
}

// 读指定用户
bool AT24C64::readUser(int index, EEP_User *user) {
    uint8_t count = getUserCount();
    if (index >= count) {
        qDebug("ERROR: 用户索引超出范围\n");
        return false;
    }

    uint16_t addr = USER_BASE_ADDR + index * USER_SIZE;
    if (!read_buf(addr, (uint8_t*)user, sizeof(EEP_User))) {
        qDebug("ERROR: 读用户失败\n");
        return false;
    }
  //  qDebug("读用户%d成功\n", index);
    return true;
}

// 获取用户总数
uint8_t AT24C64::getUserCount() {
    EEP_Head head;
    read_buf(0x0000, (uint8_t*)&head, sizeof(EEP_Head));
    return head.user_count;
}
// 删除指定用户（通过将后面的用户前移覆盖）
bool AT24C64::deleteUser(int index) {
    uint8_t count = getUserCount();
    if (index >= count) {
        qDebug("ERROR: 删除用户索引超出范围\n");
        return false;
    }

    // 将后面的用户前移
    for (int i = index; i < count - 1; i++) {
        EEP_User user;
        if (!readUser(i + 1, &user)) {
            qDebug("ERROR: 读取用户%d失败\n", i + 1);
            return false;
        }
        uint16_t addr = USER_BASE_ADDR + i * USER_SIZE;
        if (!write_buf(addr, (uint8_t*)&user, sizeof(EEP_User))) {
            qDebug("ERROR: 写入用户%d失败\n", i);
            return false;
        }
    }

    // 更新用户数
    EEP_Head head;
    read_buf(0x0000, (uint8_t*)&head, sizeof(EEP_Head));
    head.user_count--;
    write_buf(0x0000, (uint8_t*)&head, sizeof(EEP_Head));

    qDebug("删除用户%d成功，当前用户数：%d\n", index, head.user_count);
    return true;
}

// 清空所有用户
bool AT24C64::clearAllUsers() {
    EEP_Head head;
    read_buf(0x0000, (uint8_t*)&head, sizeof(EEP_Head));
    head.user_count = 0;
    if (!write_buf(0x0000, (uint8_t*)&head, sizeof(EEP_Head))) {
        qDebug("ERROR: 清空用户失败\n");
        return false;
    }
    qDebug("清空所有用户成功\n");
    return true;
}

// 同步用户列表，删除不在列表中的用户
bool AT24C64::syncUsers(QList<uint32_t> validIds) {
    uint8_t count = getUserCount();

    qDebug("开始同步，当前数据库用户数：%d\n", count);

     // 打印当前数据库中的所有用户
     if (count > 0) {
         qDebug("当前数据库用户列表：\n");
         for (int i = 0; i < count; i++) {
             EEP_User user;
             if (readUser(i, &user)) {
                 qDebug("  [%d] ID=%u, 名字=%s\n", i, user.id, user.name);
             }
         }
     }

     // 打印客户端提供的有效ID列表
     qDebug("客户端提供的有效ID列表（共%d个）：\n", validIds.size());
     for (uint32_t validId : validIds) {
         qDebug("  ID=%u\n", validId);
     }


    if (count == 0) {
        qDebug("数据库为空，无需同步\n");
        return true;
    }

    // 从后向前遍历，删除不在validIds中的用户
    int deletedCount = 0;
    for (int i = count - 1; i >= 0; i--) {
        EEP_User user;
        if (!readUser(i, &user)) {
            qDebug("ERROR: 读取用户%d失败\n", i);
            continue;
        }

        // 检查该用户ID是否在有效列表中
        bool found = false;
        for (uint32_t validId : validIds) {
            if (user.id == validId) {
                found = true;
                break;
            }
        }

        // 如果不在有效列表中，删除该用户
        if (!found) {
            qDebug("删除用户：索引=%d, ID=%u, 名字=%s\n", i, user.id, user.name);
            if (deleteUser(i)) {
                deletedCount++;
            }
        } else {
                  qDebug("保留用户：索引=%d, ID=%u, 名字=%s\n", i, user.id, user.name);
        }
    }

    qDebug("用户同步完成，删除%d个用户，剩余%d个用户\n", deletedCount, getUserCount());


    return true;
}
