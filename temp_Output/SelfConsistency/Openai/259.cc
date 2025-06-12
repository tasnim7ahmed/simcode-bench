#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Set time resolution
    Time::SetResolution(Time::NS);

    // Create nodes: 1 gNB, 2 UEs
    NodeContainer ueNodes;
    ueNodes.Create(2);
    Ptr<Node> ue1 = ueNodes.Get(0);
    Ptr<Node> ue2 = ueNodes.Get(1);

    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Mobility
    // gNB is stationary
    MobilityHelper mobilityEnb;
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    // UEs: RandomWalk2dMobilityModel
    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue(10.0),
                                   "MinY", DoubleValue(10.0),
                                   "DeltaX", DoubleValue(5.0),
                                   "DeltaY", DoubleValue(5.0),
                                   "GridWidth", UintegerValue(2),
                                   "LayoutType", StringValue("RowFirst"));
    mobilityUe.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Mode", StringValue("Time"),
        "Time", StringValue("2s"),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
        "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobilityUe.Install(ueNodes);

    // NR / mmWave EPC setup
    Ptr<mmwave::MmWaveHelper> mmwaveHelper = CreateObject<mmwave::MmWaveHelper>();
    mmwaveHelper->SetAttribute("Scheduler", StringValue("ns3::MmWaveFlexTtiMacScheduler"));
    Ptr<mmwave::MmWavePointToPointEpcHelper> epcHelper = CreateObject<mmwave::MmWavePointToPointEpcHelper>();
    mmwaveHelper->SetEpcHelper(epcHelper);

    // Core network node
    Ptr<Node> remoteHost;
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    remoteHost = remoteHostContainer.Get(0);

    // Install Internet stack on remoteHost (for the EPC/SGW/PGW)
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create the Internet backbone for PGW
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.0001)));
    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);

    // Assign IP address for core network
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    // Interface 1 is the remoteHost
    // Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Install NR Devices
    NetDeviceContainer enbDevs = mmwaveHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);

    // Install internet stack
    InternetStackHelper internetUe;
    internetUe.Install(ueNodes);

    // Attach UE devices to gNB (enb)
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        mmwaveHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // EPC assigns IP addresses to UEs
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Reassign UE IP addresses to 192.168.1.0/24
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4> ipv4Ptr = ueNodes.Get(i)->GetObject<Ipv4>();
        int32_t ifIndex = ipv4Ptr->GetInterfaceForDevice(ueDevs.Get(i));
        ipv4Ptr->SetMetric(ifIndex, 1);
        ipv4.NewAddress();
        // Note: IPs are assigned via EPC, so this is mainly documentation
        // purposes; for explicit reassignment, need custom routing.
    }

    // Enable static routing
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(
        Ipv4Address("192.168.1.0"), Ipv4Mask("255.255.255.0"), 1);

    // UDP Echo Server on UE2 (index 1)
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on UE1 (index 0), send 5 packets of 1024 bytes
    UdpEchoClientHelper echoClient(ueIpIfaces.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap (optional)
    // p2ph.EnablePcapAll("mmwave-epc");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}