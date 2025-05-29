#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: Sta1, AP (which acts as relay), Sta2
    NodeContainer staNodes;
    staNodes.Create(1);
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer sta2Node;
    sta2Node.Create(1);

    // Wi-Fi: Sta1 <-> AP
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-wifi");

    // STATION 1
    NetDeviceContainer staDevices;
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

    // AP
    NetDeviceContainer apDevices;
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    apDevices = wifi.Install(wifiPhy, wifiMac, apNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);
    mobility.Install(apNode);

    Ptr<ConstantPositionMobilityModel> staPos = staNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    staPos->SetPosition(Vector(0.0,0.0,0.0));
    Ptr<ConstantPositionMobilityModel> apPos = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
    apPos->SetPosition(Vector(5.0,0.0,0.0));

    // Point-to-Point: AP <-> Sta2
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer p2pDevices = p2p.Install(apNode, sta2Node);

    mobility.Install(sta2Node);
    Ptr<ConstantPositionMobilityModel> sta2Pos = sta2Node.Get(0)->GetObject<ConstantPositionMobilityModel>();
    sta2Pos->SetPosition(Vector(10.0,0.0,0.0));

    // Internet stack
    InternetStackHelper stack;
    stack.Install(staNodes);
    stack.Install(apNode);
    stack.Install(sta2Node);

    // Assign IP addresses
    Ipv4AddressHelper address1;
    address1.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIface, apIface;
    staIface = address1.Assign(staDevices);
    apIface = address1.Assign(apDevices);

    Ipv4AddressHelper address2;
    address2.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer apP2pIface, sta2Iface;
    apP2pIface = address2.Assign(p2pDevices.Get(0));
    sta2Iface = address2.Assign(p2pDevices.Get(1));

    // Populate routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Server on Sta2
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(sta2Node.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on Sta1, send to Sta2
    UdpClientHelper client(sta2Iface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(Seconds(0.03)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(staNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.EnablePcap("wifi-ap", apDevices.Get(0));
    wifiPhy.EnablePcap("wifi-sta1", staDevices.Get(0));
    p2p.EnablePcapAll("ap-sta2");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}