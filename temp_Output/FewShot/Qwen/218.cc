#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Simulation parameters
    double simTime = 20.0;
    uint32_t numNodes = 10;
    double distance = 500.0; // meters between nodes

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging for DSDV
    LogComponentEnable("DsdvRoutingProtocol", LOG_LEVEL_INFO);
    LogComponentEnable("DsdvHelper", LOG_LEVEL_INFO);

    // Create nodes (vehicles)
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup mobility model - vehicles moving along a straight road
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance / numNodes),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(numNodes),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Set velocity of each node to simulate movement
    for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); ++i) {
        Ptr<Node> node = *i;
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        mobility->SetPosition(Vector(rand() % static_cast<int>(distance), 0.0, 0.0));
        Ptr<ConstantVelocityMobilityModel> cvmm = DynamicCast<ConstantVelocityMobilityModel>(mobility);
        if (cvmm) {
            cvmm->SetVelocity(Vector(20.0, 0.0, 0.0)); // Moving at 20 m/s along X-axis
        }
    }

    // Setup WiFi
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.Set("ChannelWidth", UintegerValue(20));
    wifiPhy.Set("TxPowerStart", DoubleValue(5.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(5.0));
    wifiPhy.Set("TxGain", DoubleValue(0));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("CCA_Mode1_EdThreshold", DoubleValue(-89.0));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    wifiMac.SetType("ns3::AdhocWifiMac"); // No access point
    NetDeviceContainer devices = wifiMac.Install(wifiPhy, nodes);

    // Install Internet stack with DSDV routing
    DsdvHelper dsdv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(dsdv);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP echo server and client for traffic generation
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(numNodes - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(numNodes - 1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Flow monitor setup for performance metrics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation interface (optional)
    AnimationInterface anim("vanet-dsdv.xml");

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Print performance metrics
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("vanet-dsdv.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}