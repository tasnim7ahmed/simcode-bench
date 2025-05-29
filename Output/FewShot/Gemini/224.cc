#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set the number of nodes
    uint32_t numNodes = 10;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Install the Internet stack
    InternetStackHelper internet;
    AodvHelper aodv;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Configure mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
    mobility.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(nodes);

    // Create UDP client applications on each node
    uint16_t port = 9;
    for (uint32_t i = 0; i < numNodes; ++i) {
        Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
        rand->SetAttribute("Min", DoubleValue(0.0));
        rand->SetAttribute("Max", DoubleValue(numNodes - 1.0));

        TypeId tid = TypeId::LookupByName("ns3::UdpClient");
        Ptr<Application> app = CreateObjectWithAttributes<Application> ("UdpClient",
            "RemoteAddress", Ipv4AddressValue(interfaces.GetAddress(i)),
            "RemotePort", UintegerValue(port));

        Ptr<UdpClient> client = DynamicCast<UdpClient>(app);
        client->SetAttribute("PacketSize", UintegerValue(1024));
        client->SetAttribute("Interval", TimeValue(Seconds(1.0)));
        client->SetAttribute("MaxPackets", UintegerValue(20));

        Ptr<Node> node = nodes.Get(i);
        node->AddApplication(app);
        app->SetStartTime(Seconds(2.0));
        app->SetStopTime(Seconds(20.0));
    }

    // Run the simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}