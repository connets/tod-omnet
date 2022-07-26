#include <nlohmann/json.hpp>

using json = nlohmann::json;


namespace carla_api_base{

    /*Definition of all sub-components*/
    struct init_agent {
        std::string agent_id;
        std::string agent_type;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(init_agent, agent_id, agent_type)


    struct init_actor {
        std::string vehicle_id;
        std::string vehicle_type;
        std::list<init_agent> agents;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(init_actor, vehicle_id, vehicle_type, agents)


    struct init_other_actor {
        std::string actor_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(init_other_actor, actor_id)


    struct node_position {
        std::string actor_id;
        double position[3];  // x,y,z
        double velocity[3];  // x,y,z
        double rotation[3];  // pitch,yaw,roll
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(node_position, actor_id, position, velocity, rotation)

}

namespace carla_api_payload{
    /*--------------------------------------------------------------
     *                  MESSAGE PAYLOAD DEFINITIONS
     * -------------------------------------------------------------*/
    struct init{
        std::string carla_configuration;
        double carla_timestep;
        int seed;
        std::list<carla_api_base::init_actor> actors;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(init, carla_configuration, carla_timestep, seed, actors)


    struct init_completed{
        double initial_timestamp;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(init_completed, initial_timestamp)


    struct simulation_step{
        double timestep;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(simulation_step, timestep)


    struct updated_position{
        std::list<carla_api_base::node_position> nodes;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(updated_position, nodes)


    struct vehicle_status_update{
        std::string node_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vehicle_status_update, node_id)


    struct vechile_status{
        std::string node_id;
        std::string status_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vechile_status, node_id, status_id)


    struct vehicle_instruction {
        std::string node_id;
        std::string instruction_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vehicle_instruction, node_id, instruction_id)


}

namespace carla_api{
    /* MESSAGE DEFINITION*/
    /* OMNET --> CARLA*/
    struct init{
        std::string message_type = "INIT";
        carla_api_payload::init payload;
        double timestamp;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(init, message_type, payload, timestamp)

    /* CARLA --> OMNET */
    struct init_completed {
        std::string message_type = "INIT_COMPLETED";
        carla_api_payload::init_completed payload;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(init_completed, message_type, payload)


    /* OMNET --> CARLA*/
    struct simulation_step {
        std::string message_type = "SIMULATION_STEP";
        carla_api_payload::simulation_step payload;
        double timestamp;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(simulation_step, message_type, payload, timestamp)


    /* CARLA --> OMNET */
    struct updated_postion {
        std::string message_type = "UPDATED_POSITIONS";
        carla_api_payload::updated_position payload;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(updated_postion, message_type, payload)


    /* OMNET --> CARLA*/
    struct vehicle_status_update {
        std::string message_type = "VEHICLE_STATUS_UPDATE";
        carla_api_payload::vehicle_status_update payload;
        double timestamp;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vehicle_status_update, message_type, payload, timestamp)


    /* CARLA --> OMNET */
    struct vehicle_status {
        std::string message_type = "VEHICLE_STATUS";
        carla_api_payload::vechile_status payload;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vehicle_status, message_type, payload)


    /* OMNET --> CARLA*/
    struct compute_instruction {
        std::string message_type = "COMPUTE_INSTRUCTION";
        carla_api_payload::vechile_status payload;
        double timestamp;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(compute_instruction, message_type, payload, timestamp)


    /* CARLA --> OMNET */
    struct instruction {
        std::string message_type = "INSTRUCTION";
        carla_api_payload::vehicle_instruction payload;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(instruction, message_type, payload)


    /* OMNET --> CARLA*/
    struct apply_instruction {
        std::string message_type = "APPLY_INSTRUCTION";
        carla_api_payload::vehicle_instruction payload;
        double timestamp;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(apply_instruction, message_type, payload, timestamp)



    struct ok {
        std::string message_type = "OK";
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ok, message_type)

}
