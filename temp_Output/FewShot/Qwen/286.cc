#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for AODV and applications
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Setup Mobility with RandomWaypointMobilityModel
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    mobility.Install(nodes);

    // Setup WiFi
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.Set("ChannelWidth", UintegerValue(20));
    wifiPhy.Set("TxPowerStart", DoubleValue(16.0206));
    wifiPhy.Set("TxPowerEnd", DoubleValue(16.0206));
    wifiPhy.Set("TxPowerLevels", UintegerValue(1));
    wifiPhy.Set("TxGain", DoubleValue(0));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("CCA_Mode1_EdThreshold", DoubleValue(-62.0));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-96.0));
    wifiPhy.Set("FixedRss", EmptyAttributeValue());

    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    NetDeviceContainer wifiDevices = wifiMac.Install(wifiPhy, nodes);

    // Install Internet Stack with AODV
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(wifiDevices);

    // Set up UDP Echo Server on Node 4
    uint16_t port = 4000;  // UDP port number
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(4));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Set up UDP Echo Client on Node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(4), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(0)); // Unlimited packets
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // Enable PCAP tracing
    wifiPhy.EnablePcap("manet-aodv-udp", wifiDevices);

    // Run the simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}