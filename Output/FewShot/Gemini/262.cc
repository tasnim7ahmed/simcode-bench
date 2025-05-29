#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/random-walk-2d-mobility-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    NodeContainer meshNodes;
    meshNodes.Create(5);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211s);

    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.Set распространенияAttribute("HealLink", BooleanValue(false));
    mesh.Set распространенияAttribute("BeaconInterval", TimeValue(Seconds(1)));

    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    NetDeviceContainer meshDevices = mesh.Install(wifi, meshNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.Install(meshNodes);

    InternetStackHelper internet;
    internet.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(meshNodes.Get(4));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(4), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = echoClient.Install(meshNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}