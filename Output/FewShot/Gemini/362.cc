#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer apNode = NodeContainer(nodes.Get(0));
    NodeContainer staNode = NodeContainer(nodes.Get(1));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    Nssid ssid = Nssid("wifi-network");

    WifiMacHelper mac;

    mac.SetType("ns3::ApWifiMac",
                "Ssid", NssidValue(ssid));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", NssidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer::Create(apDevice, staDevice));

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(apNode);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(staNode);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}