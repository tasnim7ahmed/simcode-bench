#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/animation-interface.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetExample");

int main(int argc, char *argv[])
{
    double simulationTime = 10.0; // seconds

    // Create 3 mobile nodes
    NodeContainer manetNodes;
    manetNodes.Create(3);

    // Configure WiFi settings
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("manet-ssid");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifi.Install(phy, mac, manetNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(manetNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Echo Server on node 0
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(manetNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    // Set up UDP Echo Client on node 2 to communicate with node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(manetNodes.Get(2));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationTime));

    // Configure mobility for the nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(manetNodes);

    // Enable NetAnim for visualization
    AnimationInterface anim("manet.xml");
    anim.SetConstantPosition(manetNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(manetNodes.Get(1), 10.0, 20.0);
    anim.SetConstantPosition(manetNodes.Get(2), 20.0, 40.0);

    // Enable FlowMonitor to capture packet delivery and delay metrics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Output FlowMonitor results
    monitor->SerializeToXmlFile("manet-flowmon.xml", true, true);

    Simulator::Destroy();
    return 0;
}

