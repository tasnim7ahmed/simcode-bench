#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for relevant components
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // Create nodes: one eNodeB and one UE
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(1);

    // Install mobility model on UE (Random Walk in 50x50 area)
    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                                "Distance", DoubleValue(1.0));
    mobilityUe.Install(ueNodes);

    // Set eNodeB to be static
    MobilityHelper mobilityEnb;
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UEs to eNodeBs
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    Ipv4Address ueAddr = ueIpIface.GetAddress(0);

    Ipv4Address enbAddr = Ipv4Address("192.168.0.1");
    Ipv4InterfaceContainer enbIpIface;
    enbIpIface.Add(lteHelper->GetEpcEnbApplication(enbDevs.Get(0))->GetObject<Ipv4>()->AddInterface(enbAddr, Ipv4Mask("255.255.255.0")));

    // Set up UDP server on eNodeB at port 80
    UdpEchoServerHelper echoServer(80);
    ApplicationContainer serverApp = echoServer.Install(enbNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on UE
    UdpEchoClientHelper echoClient(enbAddr, 80);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}