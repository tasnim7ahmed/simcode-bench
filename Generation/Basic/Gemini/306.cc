#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Log configuration
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_ALL); // Uncomment for AODV debugging

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Set up node positions (straight line, 100m apart)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(100.0), // 100 meters separation
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3), // 3 nodes in a row
                                  "LayoutType", StringValue("ns3::RowFirstLayout"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Configure AODV routing protocol
    AodvHelper aodv;
    InternetStackHelper internetStack;
    internetStack.SetRoutingHelper(aodv); // Install AODV routing helper
    internetStack.Install(nodes);

    // Configure Wi-Fi (802.11g ad-hoc)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    YansWifiPhyHelper wifiPhy;
    // Set up the channel
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    // Configure MAC for ad-hoc mode
    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Assign IP addresses (10.1.1.x)
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install UDP Echo Server on Node 2 (the third node)
    UdpEchoServerHelper echoServer(9); // Port 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // Install UDP Echo Client on Node 0 (the first node)
    // Client sends to Node 2's IP address
    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0))); // 2-second intervals
    echoClient.SetAttribute("PacketSize", UintegerValue(512));   // 512 bytes
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Enable pcap tracing
    wifiPhy.EnablePcap("aodv-ad-hoc-wifi", devices);

    // Set simulation stop time
    Simulator::Stop(Seconds(20.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}