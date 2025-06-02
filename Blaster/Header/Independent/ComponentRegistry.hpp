#pragma once
#include <boost/preprocessor/cat.hpp>
#include "Independent/ECS/ComponentFactory.hpp"

template<class T>
struct _AutoComponentRegister
{
    static inline const bool value =
        []{ Blaster::Independent::ECS::ComponentFactory::Register<T>(); return true; }();
};

#define AUTO_REG_NAME BOOST_PP_CAT(_autoReg_, __COUNTER__)

#if defined(__COUNTER__)
#   define REGISTER_COMPONENT(TYPE)                                            \
static inline const bool AUTO_REG_NAME =                      \
_AutoComponentRegister < TYPE >::value; \
BOOST_CLASS_EXPORT(TYPE)
#else
#   define REGISTER_COMPONENT(TYPE)                                            \
namespace { static const bool _autoReg_ =                              \
_AutoComponentRegister < TYPE >::value; } \
BOOST_CLASS_EXPORT(TYPE)
#endif