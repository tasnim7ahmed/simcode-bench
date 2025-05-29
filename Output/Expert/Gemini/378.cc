#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/random-access-wifi-mac.h"
#include "ns3/on-off-application.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer meshNodes;
    meshNodes.Create(3);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi = WifiHelper::Default();
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    RandomAccessWifiMacHelper mac;
    MeshHelper mesh;
    mesh.SetMacType("ns3::Dot11Mac", "ActiveProbing", BooleanValue(false));

    NetDeviceContainer devices = mesh.Install(wifi, phy, mac, meshNodes);

    InternetStackHelper internet;
    internet.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(meshNodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(2), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(meshNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}