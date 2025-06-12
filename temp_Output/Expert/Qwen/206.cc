#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/propagation-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SensorNetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(6); // 5 sensors + 1 sink

    YansLrWpanHelper lrWpanHelper;
    NetDeviceContainer devices = lrWpanHelper.Install(nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(6),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(5)); // Sink node
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(5), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    for (uint32_t i = 0; i < 5; ++i) {
        ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("sensor-network.tr");
    lrWpanHelper.EnableAsciiAll(stream);

    AnimationInterface anim("sensor-network.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}