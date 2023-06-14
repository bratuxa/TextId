#include "TextId.hpp"

#include <fmt/format.h>

#include <functional>
#include <string>
#include <userver/clients/dns/component.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/assert.hpp>
#include <userver/server/http/http_request.hpp>
#include "userver/storages/postgres/cluster_types.hpp"
#include "userver/storages/postgres/exceptions.hpp"

namespace pg_service_template {

namespace {

class Hello final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler";

  Hello(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& component_context)
      : HttpHandlerBase(config, component_context),
        pg_cluster_(
            component_context
                .FindComponent<userver::components::Postgres>("postgres-db-1")
                .GetCluster()) {}

  std::string HandleRequestThrow(
  const userver::server::http::HttpRequest& request,
  userver::server::request::RequestContext&) const override {
    switch (request.GetMethod()) {
      case userver::server::http::HttpMethod::kPost:{
        const auto request_body = userver::formats::json::FromString(request.RequestBody());
        const auto user_id_hash = fmt::format("{}", std::hash<std::string>{}(fmt::format("{}", request_body["user_id"])));
        const auto paste = request_body["paste"].As<std::string>();
        auto result = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "INSERT INTO text_schema.pastes(user_id, paste, paste_id, time)"
          "VALUES ($1, $2, $3, now())"
          "RETURNING pastes.paste_id",
          user_id_hash, paste, fmt::format("{}", std::hash<std::string>{}(paste))
        );

        return fmt::format("{{\"user_id\": {},\"paste_id\": {}}}", user_id_hash, result.AsSingleRow<std::string>());
      }
      break;
      case userver::server::http::HttpMethod::kGet:{
        const auto request_body = userver::formats::json::FromString(request.RequestBody());
        const auto user_id = fmt::format("{}", request_body["user_id"]);
        const auto paste_id = fmt::format("{}", request_body["paste_id"]);

        auto result = pg_cluster_->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          "SELECT user_id, paste FROM text_schema.pastes WHERE paste_id = $1",
          paste_id
        );

        if(result.IsEmpty())
          throw userver::server::handlers::ExceptionWithCode<HandlerErrorCode::kResourceNotFound>(userver::server::handlers::ExternalBody{});

        if(user_id != result[0]["user_id"].As<std::string>())
          throw userver::server::handlers::ExceptionWithCode<HandlerErrorCode::kForbidden>(userver::server::handlers::ExternalBody{});
        
        return result[0]["paste"].As<std::string>();
      }
    }
  }

  userver::storages::postgres::ClusterPtr pg_cluster_;
};

}  // namespace

void Append(userver::components::ComponentList& component_list) {
  component_list.Append<Hello>();
  component_list.Append<userver::components::Postgres>("postgres-db-1");
  component_list.Append<userver::clients::dns::Component>();
}

}  // namespace pg_service_template
