///
/// YAMLImpl.h
/// Vorb Engine
///
/// Created by Cristian Zaloj on 21 Jan 2015
/// Copyright 2014 Regrowth Studios
/// MIT License
///
/// Summary:
/// 
///

#pragma once

#ifndef YAMLImpl_h__
#define YAMLImpl_h__

#include <yaml-cpp/yaml.h>

namespace keg {
    typedef YAML::Node YAMLNode;
    class YAMLEmitter {
    public:
        YAML::Emitter emitter;
    };
}

#endif // YAMLImpl_h__
