#ifndef AT24C64_H
#define AT24C64_H

#include <stdint.h>
#include <cstring>
#include <QDebug>
// ===================== 数据结构定义（固定地址分区） =====================
// 1. 头部信息（0x0000~0x001F，32字节）
typedef struct {
    uint16_t magic;        // 初始化标记 0x5A5A
    uint8_t  user_count;   // 已存用户数
    uint8_t  reserve[29];  // 补齐32字节
} EEP_Head;

// 2. 密钥（0x0020~0x003F，32字节）
typedef struct {
    uint8_t key[16];       // 16字节密钥
    uint8_t reserve[16];   // 补齐32字节
} EEP_Key;

// 3. 管理员密码（0x0040~0x005F，32字节）
typedef struct {
    uint8_t pwd_hash[32];       // 20字节密码（ASCII）
//    uint8_t crc;  // CRC8校验位（补充该成员）
//    uint8_t reserve[11];   // 补齐32字节
} EEP_Pwd;

// 4. 系统参数（0x0060~0x007F，32字节）
typedef struct {
    uint8_t unlock_mode;   // 开锁模式：0-密码 1-刷卡
    uint8_t beep_enable;   // 蜂鸣器：0-关 1-开
    uint8_t reserve[30];   // 补齐32字节
} EEP_Param;

// 5. 用户信息（0x0080开始，每条32字节）
typedef struct {
    uint8_t name[16];      // 人名（ASCII，不足补0）
    uint32_t id;           // 对应ID
    uint8_t reserve[12];   // 补齐32字节
} EEP_User;

// 宏定义
#define AT24C64_I2C_ADDR    0x50    // 正点原子默认7位地址
#define AT24C64_PAGE_SIZE   32
#define I2C_DEV             "/dev/i2c-0"
#define USER_BASE_ADDR      0x0080
#define USER_SIZE           32

// ===================== AT24C64驱动类 =====================
class AT24C64
{
public:
    AT24C64();
    ~AT24C64();

    // 初始化硬件I2C
    bool init();

    // 基础读写
    uint8_t read(uint16_t addr);
    bool write(uint16_t addr, uint8_t data);
    bool read_buf(uint16_t addr, uint8_t *buf, int len);
    bool write_buf(uint16_t addr, const uint8_t *buf, int len);

    // 业务层读写（密钥/密码/参数/用户）
    bool initEEPROM();                // 初始化EEPROM（首次用）
    bool writeKey(EEP_Key *key);      // 写密钥
    bool readKey(EEP_Key *key);       // 读密钥
    bool writePwd(EEP_Pwd *pwd);      // 写密码
    bool readPwd(EEP_Pwd *pwd);       // 读密码
    bool writeParam(EEP_Param *param);// 写参数
    bool readParam(EEP_Param *param); // 读参数
    bool addUser(EEP_User *user);     // 新增用户
    bool readUser(int index, EEP_User *user); // 读指定用户
    uint8_t getUserCount();           // 获取用户总数

    bool deleteUser(int index);       // 删除指定用户
     bool clearAllUsers();             // 清空所有用户
     bool syncUsers(QList<uint32_t> validIds); // 同步用户列表，删除不在列表中的用户

private:
    int fd; // I2C文件描述符
};

#endif // AT24C64_H
