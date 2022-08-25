#include <nlohmann/json.hpp>

using json = nlohmann::json;

#define SIM_STATUS_RUNNING 0
#define SIM_STATUS_FINISHED 1
#define SIM_STATUS_ERROR -1



namespace carla_api_base{

    /*Definition of all sub-components*/
    struct init_agent {
        std::string agent_id;
        std::string agent_configuration;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(init_agent, agent_id, agent_configuration)


    struct init_actor {
        std::string actor_id;
        std::string actor_configuration;
        std::string route;
        std::list<init_agent> agents;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(init_actor, actor_id, actor_configuration, route, agents)


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
        std::string carla_world_configuration;
        double carla_timestep;
        double sim_time_limit;
        int seed;
        std::string run_id;
        std::list<carla_api_base::init_actor> actors;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(init, carla_world_configuration, carla_timestep, sim_time_limit, seed ,run_id, actors)


    struct init_completed{
        double initial_timestamp;
        std::list<carla_api_base::node_position> actors;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(init_completed, initial_timestamp, actors)


    struct simulation_step{
        double timestep;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(simulation_step, timestep)


    struct updated_position{
        std::list<carla_api_base::node_position> actors;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(updated_position, actors)


    struct vehicle_status_update{
        std::string actor_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vehicle_status_update, actor_id)


    struct vechile_status{
        std::string actor_id;
        std::string status_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vechile_status, actor_id, status_id)


    struct compute_instuction{
        std::string actor_id;
        std::string agent_id;
        std::string status_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(compute_instuction, actor_id, agent_id, status_id)


    struct vehicle_instruction {
        std::string actor_id;
        std::string instruction_id;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vehicle_instruction, actor_id, instruction_id)


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
        int simulation_status;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(init_completed, message_type, payload, simulation_status)


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
        int simulation_status;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(updated_postion, message_type, payload, simulation_status)


    /* OMNET --> CARLA*/
    struct vehicle_status_update {
        std::string message_type = "ACTOR_STATUS_UPDATE";
        carla_api_payload::vehicle_status_update payload;
        double timestamp;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vehicle_status_update, message_type, payload, timestamp)


    /* CARLA --> OMNET */
    struct vehicle_status {
        std::string message_type = "ACTOR_STATUS";
        carla_api_payload::vechile_status payload;
        int simulation_status;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vehicle_status, message_type, payload, simulation_status)


    /* OMNET --> CARLA*/
    struct compute_instruction {
        std::string message_type = "COMPUTE_INSTRUCTION";
        carla_api_payload::compute_instuction payload;
        double timestamp;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(compute_instruction, message_type, payload, timestamp)


    /* CARLA --> OMNET */
    struct instruction {
        std::string message_type = "INSTRUCTION";
        carla_api_payload::vehicle_instruction payload;
        int simulation_status;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(instruction, message_type, payload, simulation_status)


    /* OMNET --> CARLA*/
    struct apply_instruction {
        std::string message_type = "APPLY_INSTRUCTION";
        carla_api_payload::vehicle_instruction payload;
        double timestamp;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(apply_instruction, message_type, payload, timestamp)



    struct ok {
        std::string message_type = "OK";
        int simulation_status;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ok, message_type, simulation_status)

}
