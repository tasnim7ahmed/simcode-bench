#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-epc-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    // Create nodes: 1 eNodeB, 3 UEs
    NodeContainer ueNodes;
    ueNodes.Create(3);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install LTE Devices to the nodes
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    lteHelper->SetEpcHelper(epcHelper);

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Set up the mobility - eNodeB stationary at center, UEs mobile in 100x100
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
    enbPositionAlloc->Add(Vector(50.0, 50.0, 0.0));
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue("Time"),
                                "Time", TimeValue(Seconds(0.5)),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"),
                                "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)));
    ueMobility.Install(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
    // Attach all UEs to the first eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Application configuration
    // UE0: UDP Server on port 5000
    uint16_t port = 5000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UE1: UDP Client to UE0, port 5000, 512-byte packets every 10ms
    UdpClientHelper udpClient(ueIpIfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    lteHelper->EnableTraces();
    epcHelper->EnablePcap("lte-epc");
    // Also enable Data Channel PCAP
    lteHelper->EnableDlPhyTraces();
    lteHelper->EnableUlPhyTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}