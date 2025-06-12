#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsdv-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETDSDVSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: 5 vehicles + 1 RSU
    NodeContainer nodes;
    nodes.Create(6);
    Ptr<Node> rsuNode = nodes.Get(5);

    // Setup DSDV routing
    dsdv::DsdvHelper dsdv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(dsdv);
    stack.Install(nodes);

    // Create a point-to-point channel (used for logging and topology)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < 5; ++i) {
        devices.Add(p2p.Install(nodes.Get(i), rsuNode));
    }

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(nodes);

    // Set waypoints for each vehicle along a highway
    Ptr<WaypointMobilityModel> waypoint;
    for (uint32_t i = 0; i < 5; ++i) {
        waypoint = DynamicCast<WaypointMobilityModel>(nodes.Get(i)->GetObject<MobilityModel>());
        waypoint->AddWaypoint(Waypoint(Seconds(0), Vector(0, i * 10, 0)));
        waypoint->AddWaypoint(Waypoint(Seconds(30), Vector(1000, i * 10, 0))); // Move to 1000m over 30s
    }

    // UDP echo server on the RSU
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(rsuNode);
    serverApps.Start(Seconds(0));
    serverApps.Stop(Seconds(30));

    // UDP clients in vehicles sending packets to RSU
    uint32_t packetSize = 1024;
    Time interPacketInterval = Seconds(1.);
    for (uint32_t i = 0; i < 5; ++i) {
        UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9); // RSU's IP and port
        echoClient.SetAttribute("MaxPackets", UintegerValue(30));
        echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(30.0));
    }

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("vanet-dsdv-simulation.tr"));

    // Enable PCAP tracing
    p2p.EnablePcapAll("vanet-dsdv-simulation");

    Simulator::Stop(Seconds(30));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}