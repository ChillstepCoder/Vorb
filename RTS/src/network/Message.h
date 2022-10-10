#pragma once

#include "network/NetworkConst.h"

// ==============================================
// =           MESSAGE TYPES                    =
// ==============================================
enum class MessageTypes {
    PING,
    CLIENT_JOIN, // Cli -> Srv
    CLIENT_BEGIN, // Srv -> Cli
    ENTITY_CREATE, // Srv -> Cli
    ENTITY_DESTROY, // Srv -> Cli
    ENTITY_TRANSFORM, // Srv -> Cli
    COUNT
};

// ==============================================
// =           MESSAGE CHANNELS                 =
// ==============================================
constexpr GameChannel MESSAGE_CHANNELS[e_cast(MessageTypes::COUNT)] = {
    GameChannel::UNRELIABLE, // PING,
    GameChannel::RELIABLE,   // CLIENT_JOIN,
    GameChannel::RELIABLE,   // CLIENT_BEGIN,
    GameChannel::RELIABLE,   // ENTITY_CREATE,
    GameChannel::RELIABLE,   // ENTITY_DESTROY,
    GameChannel::UNRELIABLE, // ENTITY_TRANSFORM,
};
static_assert(e_cast(MessageTypes::COUNT) == 6, "Make sure to set the channel and check order");

// Base message type, all definitions should inherit
struct MessageBase : public yojimbo::Message {
    GameChannel getChannel() const { return MESSAGE_CHANNELS[GetType()]; }
};

// ==============================================
// =           MESSAGE DEFINITIONS              =
// ==============================================
struct PingMessage : public MessageBase {

    template <typename Stream> bool Serialize(Stream& stream) {
        serialize_double(stream, mTimeStamp);
        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    f64 mTimeStamp;
    // bool mServerOrigin;
};

struct ClientJoinMessage : public MessageBase {

    template <typename Stream> bool Serialize(Stream& stream) {

        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    // ui64 SteamID
};

struct ClientBeginMessage : public MessageBase {

    template <typename Stream> bool Serialize(Stream& stream) {
        serialize_float(stream, mPosition.x);
        serialize_float(stream, mPosition.y);
        serialize_float(stream, mPosition.z);
        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    f32v3 mPosition;
    // TODO: Client data?
};

struct EntityCreateMessage : public MessageBase {

    template <typename Stream> bool Serialize(Stream& stream) {
        // TODO: Allow server to serialize without checks?
        serialize_float(stream, mPosition.x);
        serialize_float(stream, mPosition.y);
        serialize_float(stream, mPosition.z);
        serialize_float(stream, mRotation);
        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    entt::entity entityID;
    f32v3 mPosition = f32v3(0.0f);
    f32 mRotation = 0.0f;

};

struct EntityDestroyMessage : public MessageBase {

    template <typename Stream> bool Serialize(Stream& stream) {
        // TODO: Allow server to serialize without checks?
        assert(false);
        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    entt::entity entityID;

};

struct EntityTransformMessage : public MessageBase {

    template <typename Stream> bool Serialize(Stream& stream) {
        // TODO: Allow server to serialize without checks?
        serialize_float(stream, mRotation);
        serialize_float(stream, mPosition.x);
        serialize_float(stream, mPosition.y);
        serialize_float(stream, mPosition.z);
        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    // TODO: Quantize
    f32 mRotation = 0.0f;
    f32v3 mPosition = f32v3(0.0f);
};


// ==============================================
// =           MESSAGE FACTORY                  =
// ==============================================
YOJIMBO_MESSAGE_FACTORY_START(GameMessageFactory, (int)MessageTypes::COUNT);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::PING, PingMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::CLIENT_JOIN, ClientJoinMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::CLIENT_BEGIN, ClientBeginMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::ENTITY_CREATE, EntityTransformMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::ENTITY_DESTROY, EntityTransformMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::ENTITY_TRANSFORM, EntityTransformMessage);
YOJIMBO_MESSAGE_FACTORY_FINISH();

static_assert(e_cast(MessageTypes::COUNT) == 6, "Update message factory with definitions");