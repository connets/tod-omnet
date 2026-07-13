#include <nlohmann/json.hpp>

using json = nlohmann::json;


namespace tod_carla_api{
    /* OMNET --> CARLA*/
    struct actor_status_update {
        std::string user_message_type = "ACTOR_STATUS_UPDATE";
        std::string actor_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(actor_status_update, user_message_type, actor_id)

    /* OMNET --> CARLA*/
    struct actor_status_update_zero_delay {
        std::string user_message_type = "ACTOR_STATUS_UPDATE_ZERO_DELAY";
        std::string actor_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(actor_status_update_zero_delay, user_message_type, actor_id)


    /* CARLA --> OMNET */
    struct actor_status {
        std::string user_message_type = "ACTOR_STATUS";
        std::string actor_id; //TODO: remove because is unused
        std::string status_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(actor_status, user_message_type, actor_id, status_id)


    /* OMNET --> CARLA*/
    struct compute_instruction {
        std::string user_message_type = "COMPUTE_INSTRUCTION";
        std::string actor_id;
        std::string agent_id;
        std::string status_id;
        double loss_ratio = 0.0;   // frazione di dati-sensore mancanti nel frame (0 = completo)
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(compute_instruction, user_message_type, actor_id, agent_id, status_id, loss_ratio)


    /* CARLA --> OMNET */
    struct instruction {
        std::string user_message_type = "INSTRUCTION";
        std::string actor_id;
        std::string instruction_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(instruction, user_message_type, actor_id, instruction_id)


    /* OMNET --> CARLA*/
    struct apply_instruction {
        std::string user_message_type = "APPLY_INSTRUCTION";
        std::string actor_id;
        std::string instruction_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(apply_instruction, user_message_type, actor_id, instruction_id)


    struct ok {
        std::string user_message_type = "OK";
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ok, user_message_type)




    /* OMNET --> CARLA*/ /* CARLA -actor_status-> OMNET */
    struct cooperative_status_request {
        std::string user_message_type = "COOPERATIVE_STATUS_REQUEST";
        std::string actor_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(cooperative_status_request, user_message_type, actor_id)

    /* OMNET --> CARLA*/ /* CARLA -ok-> OMNET */
    struct cooperative_update {
        std::string user_message_type = "COOPERATIVE_UPDATE";
        std::string actor_id;
        std::string status_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(cooperative_update, user_message_type, actor_id, status_id)

}
