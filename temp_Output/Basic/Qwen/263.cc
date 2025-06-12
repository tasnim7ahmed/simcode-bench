#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(1);

    // Setup LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("UseRlcSm", BooleanValue(true));

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes.Get(0));
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes.Get(0));

    // Install the IP stack on the UE and eNodeB
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIfaces = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    Ipv4Address ueIp = ueIpIfaces.GetAddress(0);

    // Attach UE to eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Set mobility for UE
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=-50.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(ueNodes);

    // Install UDP echo server on UE
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP echo client on eNodeB
    UdpEchoClientHelper echoClient(ueIp, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}