#pragma once

#include "Independent/ECS/ComponentFactory.hpp"
#include "Independent/Utility/TypeRegistrar.hpp"
#include <boost/preprocessor/cat.hpp>

template<class T>
struct _AutoComponentRegister
{
#ifndef _MSC_VER
    static inline const bool value [[gnu::used]] = []{ Blaster::Independent::ECS::ComponentFactory::Register<T>(); return true; }();
#else
    static inline const bool value = []{ Blaster::Independent::ECS::ComponentFactory::Register<T>(); return true; }();
#endif
};

#define AUTO_REG_NAME BOOST_PP_CAT(_autoReg_, __COUNTER__)

#if defined(__COUNTER__)
#   define REGISTER_COMPONENT(TYPE, ID)                                            \
static inline const bool AUTO_REG_NAME =                      \
_AutoComponentRegister < TYPE >::value; \
BOOST_CLASS_EXPORT(TYPE)  \
REGISTER_TYPE(TYPE, ID)
#else
#   define REGISTER_COMPONENT(TYPE)                                            \
namespace { static const bool _autoReg_ =                              \
_AutoComponentRegister < TYPE >::value; } \
BOOST_CLASS_EXPORT(TYPE)
#endif