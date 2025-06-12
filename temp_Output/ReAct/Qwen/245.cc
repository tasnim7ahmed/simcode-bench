#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/object-name-service.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NodeNamingExample");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(5);

    ObjectNameService::SetObjectName(nodes.Get(0), "Router");
    ObjectNameService::SetObjectName(nodes.Get(1), "Server");
    ObjectNameService::SetObjectName(nodes.Get(2), "Client1");
    ObjectNameService::SetObjectName(nodes.Get(3), "Client2");
    ObjectNameService::SetObjectName(nodes.Get(4), "Firewall");

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        std::string name = ObjectNameService::GetObjectName(node);
        NS_LOG_UNCOND("Node ID: " << node->GetId() << ", Name: " << name);
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}