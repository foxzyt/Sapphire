#pragma once

#include <string>
#include <memory>
#include "error.h"

// Injects extremely friendly, Rust-style hints to Sapphire Error objects
void inject_syntax_hints(const std::string& message, std::shared_ptr<SapphireError>& error);
void inject_runtime_hints(const std::string& message, std::shared_ptr<SapphireError>& error, void* vm_ptr);
