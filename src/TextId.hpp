#pragma once

#include <string>
#include <string_view>

#include <userver/components/component_list.hpp>

namespace pg_service_template {

enum class UserType { kFirstTime, kKnown };

void Append(userver::components::ComponentList& component_list);

}  // namespace pg_service_template
