/**
 * @file protocol.cpp
 * @brief 协议实现
 */

#include "protocol.h"
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QIODevice>

QByteArray Protocol::encode(MsgType type, const std::string& body) {
    QByteArray result;

    // 消息头：2字节类型 + 4字节长度（大端序）
    quint16 msg_type = static_cast<quint16>(type);
    quint32 length = static_cast<quint32>(body.size());

    // 添加头部 - QDataStream 会自动处理字节序转换
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << msg_type << length;

    // 添加消息体
    if (!body.empty()) {
        result.append(body.data(), body.size());
    }

    return result;
}

QByteArray Protocol::encode(MsgType type, const QString& body) {
    return encode(type, body.toStdString());
}

bool Protocol::decode(QByteArray& data, MsgType& type, QString& body) {
    if (data.size() < 6) {
        return false;  // 数据不足
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);

    // 读取头部 - QDataStream 会自动处理字节序转换
    quint16 msg_type;
    quint32 length;
    stream >> msg_type >> length;

    type = static_cast<MsgType>(msg_type);

    // 检查数据是否完整
    if (data.size() < 6 + static_cast<int>(length)) {
        return false;
    }

    // 读取消息体
    if (length > 0) {
        body = QString::fromUtf8(data.constData() + 6, length);
    } else {
        body = "";
    }

    // 解码成功后，移除已处理的数据
    int header_size = 6;
    int total_size = header_size + static_cast<int>(length);
    data.remove(0, total_size);

    return true;
}

QString Protocol::generateMsgId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
}

QString Protocol::makeLoginRequest(const QString& user_id, const QString& password) {
    QJsonObject obj;
    obj["user_id"] = user_id;
    obj["password"] = password;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QString Protocol::makeChatMessage(const QString& from, const QString& to,
                                  const QString& content_type, const QString& content) {
    return makeChatMessage(generateMsgId(), from, to, content_type, content);
}

QString Protocol::makeChatMessage(const QString& msg_id, const QString& from, const QString& to,
                                  const QString& content_type, const QString& content) {
    QJsonObject obj;
    obj["msg_id"] = msg_id;
    obj["from_user_id"] = from;
    obj["to_user_id"] = to;
    obj["content_type"] = content_type;
    obj["content"] = content;
    obj["client_time"] = QDateTime::currentSecsSinceEpoch();
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

bool Protocol::parseLoginResponse(const QString& body, LoginResponse& rsp) {
    QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8());
    if (!doc.isObject()) {
        return false;
    }

    QJsonObject obj = doc.object();
    rsp.code = obj["code"].toInt();
    rsp.message = obj["message"].toString().toStdString();
    rsp.user_id = obj["user_id"].toString().toStdString();
    rsp.nickname = obj["nickname"].toString().toStdString();
    rsp.avatar_url = obj["avatar_url"].toString().toStdString();
    rsp.gender = obj["gender"].toString().toStdString();
    rsp.region = obj["region"].toString().toStdString();
    rsp.signature = obj["signature"].toString().toStdString();
    rsp.token = obj["token"].toString().toStdString();

    return true;
}
