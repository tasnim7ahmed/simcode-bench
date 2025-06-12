#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    NodeContainer meshNodes;
    meshNodes.Create(4);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211_11A);

    MeshHelper mesh;
    mesh.SetStackInstaller(InternetStackHelper());
    mesh.SetWifiPhy("Ht80");
    mesh.SetMacType("RandomStart");

    NetDeviceContainer devices = mesh.Install(wifi, meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(meshNodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient1(interfaces.GetAddress(3), 9);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp1 = echoClient1.Install(meshNodes.Get(0));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(interfaces.GetAddress(3), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp2 = echoClient2.Install(meshNodes.Get(1));
    clientApp2.Start(Seconds(2.0));
    clientApp2.Stop(Seconds(10.0));

     UdpEchoClientHelper echoClient3(interfaces.GetAddress(3), 9);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp3 = echoClient3.Install(meshNodes.Get(2));
    clientApp3.Start(Seconds(2.0));
    clientApp3.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}