#pragma once

struct SimFullEntityBinding;

// If this exists, we are full simulated
struct FullEntityBindingComponent {
    SimFullEntityBinding* binding;
};