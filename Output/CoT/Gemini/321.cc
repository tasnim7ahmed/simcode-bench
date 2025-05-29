#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mesh-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <sstream>

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer meshNodes;
    meshNodes.Create(4);

    MeshHelper meshHelper;
    meshHelper.SetStandard(WIFI_PHY_STANDARD_80211s);
    meshHelper.SetBeaconInterval(Seconds(1));
    meshHelper.SetRoutingProtocol("ns3::AodvMeshRouting");

    NetDeviceContainer meshDevices = meshHelper.Install(meshNodes);

    InternetStackHelper internet;
    internet.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(meshNodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(3), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = client.Install(meshNodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    ApplicationContainer clientApps2 = client.Install(meshNodes.Get(1));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(10.0));

    ApplicationContainer clientApps3 = client.Install(meshNodes.Get(2));
    clientApps3.Start(Seconds(2.0));
    clientApps3.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}