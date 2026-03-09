#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>

class ConfigManager
{
public:
    static ConfigManager& instance();

    QString getFaceLandmarkModelPath() const;
    QString getFaceRecognitionModelPath() const;
    QString getTestImagePath() const;
    QString getTestOutputPath() const;
    QString getServerIp() const;
    quint16 getTcpPort() const;

    void setFaceLandmarkModelPath(const QString& path);
    void setFaceRecognitionModelPath(const QString& path);
    void setTestImagePath(const QString& path);
    void setTestOutputPath(const QString& path);
    void setServerIp(const QString& ip);
    void setTcpPort(quint16 port);

    void save();
    void load();

private:
    ConfigManager();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    QString getConfigFilePath() const;

    QSettings* m_settings;
    QString m_faceLandmarkModelPath;
    QString m_faceRecognitionModelPath;
    QString m_testImagePath;
    QString m_testOutputPath;
    QString m_serverIp;
    quint16 m_tcpPort;
};

#endif // CONFIGMANAGER_H
