// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_OPERATORS_RX_MEMBERS_EMPTY_HPP)
#define RXCPP_OPERATORS_RX_MEMBERS_EMPTY_HPP

#include "../rx-includes.hpp"

namespace rxcpp {

template<class T, class SourceOperator, class Observable>
class observable_operators
{
};

template<>
class observable<void, void>
{
    ~observable();
};

}

#endif
