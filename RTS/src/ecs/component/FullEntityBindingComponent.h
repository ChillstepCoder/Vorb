#pragma once

class SimFullEntityBinding;

// If this exists, we are full simulated, or pending refcount
// Provides interface for passing data from sim -> game thread
struct FullEntityBindingComponent {
    SimFullEntityBinding* binding;
};