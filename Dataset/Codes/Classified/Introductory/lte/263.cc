#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create the nodes (eNodeB and UE)
    NodeContainer enbNode;
    enbNode.Create(1); // Create 1 eNodeB
    NodeContainer ueNodes;
    ueNodes.Create(1); // Create 1 UE

    // Set up LTE helper and Internet stack
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNode);

    // Set up mobility for eNodeB and UE
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNode);
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(ueNodes);

    // Install LTE devices (eNodeB and UE)
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UE to the eNodeB
    lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Set up IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface = address.Assign(ueDevs);

    // Set up a simple UDP echo server and client
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0)); // Install on the UE node
    serverApps.Start(Seconds(2.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(ueIpIface.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(enbNode.Get(0)); // Install on the eNodeB node
    clientApps.Start(Seconds(3.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
