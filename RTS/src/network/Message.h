#pragma once

#include "network/NetworkConst.h"
#include "util/StrToken.h"

// ==============================================
// =           MESSAGE TYPES                    =
// ==============================================
enum class MessageTypes {
    PING,
    CLIENT_READY_JOIN, // Cli -> Srv
    CLIENT_BEGIN, // Srv -> Cli
    ENTITY_CREATE, // Srv -> Cli
    ENTITY_DESTROY, // Srv -> Cli
    ENTITY_TRANSFORM, // Srv -> Cli
    CHARACTER_STATE, // Srv -> Cli
    CLIENT_PLAYER_STATE, // Cli -> Srv
    COUNT
};

// ==============================================
// =           MESSAGE CHANNELS                 =
// ==============================================
constexpr GameChannel MESSAGE_CHANNELS[e_cast(MessageTypes::COUNT)] = {
    GameChannel::UNRELIABLE, // PING,
    GameChannel::RELIABLE,   // CLIENT_READY_JOIN,
    GameChannel::RELIABLE,   // CLIENT_BEGIN,
    GameChannel::RELIABLE,   // ENTITY_CREATE,
    GameChannel::RELIABLE,   // ENTITY_DESTROY,
    GameChannel::UNRELIABLE, // ENTITY_TRANSFORM,
    GameChannel::UNRELIABLE, // CHARACTER_STATE,
    GameChannel::UNRELIABLE, // CLIENT_PLAYER_STATE,
};
static_assert(e_cast(MessageTypes::COUNT) == 8, "Make sure to set the channel and check order");

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

struct ClientReadyJoinMessage : public MessageBase {

    template <typename Stream> bool Serialize(Stream& stream) {
        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();
};

struct ClientBeginMessage : public MessageBase {

    template <typename Stream> bool Serialize(Stream& stream) {
        serialize_uint32(stream, mSrvEntityID);
        serialize_float(stream, mPosition.x);
        serialize_float(stream, mPosition.y);
        serialize_float(stream, mPosition.z);
        serialize_float(stream, mRotation);
        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    ui32 mSrvEntityID;
    f32v3 mPosition = f32v3(0.0f);
    f32 mRotation = 0.0f;
    // TODO: player data?
};

struct EntityCreateMessage : public MessageBase {

    template <typename Stream> bool Serialize(Stream& stream) {
        // TODO: Allow server to serialize without checks?
        serialize_uint32(stream, mSrvEntityID);
        serialize_float(stream, mPosition.x);
        serialize_float(stream, mPosition.y);
        serialize_float(stream, mPosition.z);
        serialize_float(stream, mRotation);
        serialize_uint64(stream, mEntityToken.mToken);
        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    ui32 mSrvEntityID;
    f32v3 mPosition = f32v3(0.0f);
    f32 mRotation = 0.0f;
    StrToken mEntityToken;

};

struct EntityDestroyMessage : public MessageBase {

    template <typename Stream> bool Serialize(Stream& stream) {
        serialize_uint32(stream, mSrvEntityID);
        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    ui32 mSrvEntityID;

};

struct EntityTransformMessage : public MessageBase {

    template <typename Stream> bool Serialize(Stream& stream) {
        // TODO: Allow server to serialize without checks?
        serialize_uint32(stream, mSrvEntityID);
        serialize_float(stream, mRotation);
        serialize_float(stream, mPosition.x);
        serialize_float(stream, mPosition.y);
        serialize_float(stream, mPosition.z);
        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    // TODO: Quantize
    ui32 mSrvEntityID;
    f32 mRotation = 0.0f;
    f32v3 mPosition = f32v3(0.0f);
};

struct CharacterStateMessage : public MessageBase {

    template <typename Stream> bool Serialize(Stream& stream) {
        // TODO: Allow server to serialize without checks?
        serialize_uint32(stream, mSrvEntityID);
        serialize_float(stream, mControlDirection.x);
        serialize_float(stream, mControlDirection.y);
        serialize_float(stream, mPosition.x);
        serialize_float(stream, mPosition.y);
        serialize_float(stream, mPosition.z);
        serialize_float(stream, mVelocity.x);
        serialize_float(stream, mVelocity.y);
        serialize_float(stream, mVelocity.z);
        serialize_uint32(stream, mDesiredLocomotionMode);
        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    // TODO: Quantize
    ui32 mSrvEntityID;
    f32v2 mControlDirection = f32v2(0.0f);
    f32v3 mPosition = f32v3(0.0f);
    f32v3 mVelocity = f32v3(0.0f);
    ui32 mDesiredLocomotionMode = 0; // TODO: investigate byte packing
};

struct ClientPlayerStateMessage : public MessageBase {

    template <typename Stream> bool Serialize(Stream& stream) {
        // TODO: Allow server to serialize without checks?
        serialize_float(stream, mControlDirection.x);
        serialize_float(stream, mControlDirection.y);
        serialize_float(stream, mPosition.x);
        serialize_float(stream, mPosition.y);
        serialize_float(stream, mPosition.z);
        serialize_float(stream, mVelocity.x);
        serialize_float(stream, mVelocity.y);
        serialize_float(stream, mVelocity.z);
        serialize_uint32(stream, mDesiredLocomotionMode);
        return true;
    }

    YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    // TODO: Quantize
    f32v2 mControlDirection = f32v2(0.0f);
    f32v3 mPosition = f32v3(0.0f);
    f32v3 mVelocity = f32v3(0.0f);
    ui32 mDesiredLocomotionMode = 0; // TODO: investigate byte packing
};


// ==============================================
// =           MESSAGE FACTORY                  =
// ==============================================
YOJIMBO_MESSAGE_FACTORY_START(GameMessageFactory, (int)MessageTypes::COUNT);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::PING, PingMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::CLIENT_READY_JOIN, ClientReadyJoinMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::CLIENT_BEGIN, ClientBeginMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::ENTITY_CREATE, EntityCreateMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::ENTITY_DESTROY, EntityDestroyMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::ENTITY_TRANSFORM, EntityTransformMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::CHARACTER_STATE, CharacterStateMessage);
YOJIMBO_DECLARE_MESSAGE_TYPE((int)MessageTypes::CLIENT_PLAYER_STATE, ClientPlayerStateMessage);
YOJIMBO_MESSAGE_FACTORY_FINISH();

static_assert(e_cast(MessageTypes::COUNT) == 8, "Update message factory with definitions");