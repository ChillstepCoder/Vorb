#pragma once

#include "boost/container/flat_set.hpp"

struct ContractHolderComponent {
    boost::container::flat_set<ContractID> mHeldContracts;
};
