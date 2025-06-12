#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/zigbee-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ZigBeeWSNSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 10;
    double simulationTime = 10.0; // seconds
    uint32_t packetSize = 128;
    Time interval = MilliSeconds(500);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    ZigbeeHelper zigbee;
    zigbee.EnableLogComponents();
    zigbee.SetZigbeeProMode();

    NetDeviceContainer devices = zigbee.Install(nodes);
    zigbee.AssignPanId(devices, 0x1234);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    ApplicationContainer apps;

    for (uint32_t i = 1; i < numNodes; ++i) {
        InetSocketAddress dest(interfaces.GetAddress(0), 9); // Coordinator's port
        UdpEchoClientHelper client(dest, packetSize);
        client.SetAttribute("MaxPackets", UintegerValue((simulationTime * 1000) / interval.GetMilliSeconds()));
        client.SetAttribute("Interval", TimeValue(interval));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        apps.Add(client.Install(nodes.Get(i)));
    }

    UdpEchoServerHelper server(9);
    apps.Add(server.Install(nodes.Get(0)));

    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(simulationTime));

    AsciiTraceHelper ascii;
    zigbee.EnableAsciiAll(ascii.CreateFileStream("zigbee-wsn.tr"));
    zigbee.EnablePcapAll("zigbee-wsn");

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}