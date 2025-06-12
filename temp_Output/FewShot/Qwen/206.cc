#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for LR-WPAN and UDP applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("LrWpanMac", LOG_LEVEL_ALL);
    LogComponentEnable("LrWpanCsmaCa", LOG_LEVEL_ALL);
    LogComponentEnable("LrWpanPhy", LOG_LEVEL_ALL);

    // Create 6 nodes: 5 sensors + 1 sink
    NodeContainer sensorNodes;
    sensorNodes.Create(5);
    NodeContainer sinkNode;
    sinkNode.Create(1);

    // Install LR-WPAN devices
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer sensorDevices;
    NetDeviceContainer sinkDevice;

    sensorDevices = lrWpanHelper.Install(sensorNodes);
    sinkDevice = lrWpanHelper.Install(sinkNode);

    // Set channel parameters
    lrWpanHelper.SetChannelParameter("Delay", StringValue("2ms"));
    lrWpanHelper.SetChannelParameter("DataRate", StringValue("250kbps"));

    // Mobility: Star topology with sink at center
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensorNodes);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(50.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(0.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(sinkNode);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(NodeContainer::GetGlobal());

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer sensorInterfaces;
    Ipv4InterfaceContainer sinkInterface;

    sensorInterfaces = ipv4.Assign(sensorDevices);
    sinkInterface = ipv4.Assign(sinkDevice);

    // Set up UDP Echo Server on sink node
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(sinkNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Set up UDP clients on all sensor nodes
    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i) {
        UdpEchoClientHelper echoClient(sinkInterface.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(128));

        ApplicationContainer clientApp = echoClient.Install(sensorNodes.Get(i));
        clientApp.Start(Seconds(2.0 + i));  // staggered start times
        clientApp.Stop(Seconds(20.0));
    }

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup NetAnim visualization
    AnimationInterface anim("sensor-network.xml");
    anim.SetMobilityPollInterval(Seconds(1));

    // Add node descriptions
    anim.UpdateNodeDescription(sinkNode.Get(0), "Sink");
    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(sensorNodes.Get(i), "Sensor-" + std::to_string(i));
    }

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}