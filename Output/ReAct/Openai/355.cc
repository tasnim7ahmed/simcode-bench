#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(3);

    // Configure the physical layer and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi as 802.11n (Wi-Fi 4)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"));

    // Configure MAC as Adhoc for all nodes
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // Install devices
    NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

    // Set mobility: Grid position for the nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(10.0),
        "DeltaY", DoubleValue(10.0),
        "GridWidth", UintegerValue(2),
        "LayoutType", StringValue("RowFirst")
    );
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Echo Server on node 0
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 2, targeting node 0
    UdpEchoClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(wifiNodes.Get(2));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing if needed
    // phy.EnablePcapAll("wifi-udp");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}