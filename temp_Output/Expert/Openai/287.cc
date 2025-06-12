#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create Nodes: 1 gNB, 2 UEs
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer gnbNodes;
    gnbNodes.Create(1);

    // Install Mobility
    // gNB: stationary at (25,25)
    Ptr<ListPositionAllocator> gnbPos = CreateObject<ListPositionAllocator>();
    gnbPos->Add(Vector(25.0, 25.0, 3.0));
    MobilityHelper gnbMob;
    gnbMob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMob.SetPositionAllocator(gnbPos);
    gnbMob.Install(gnbNodes);

    // UEs: Random walk in 50x50m area
    MobilityHelper ueMob;
    ueMob.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                           "Bounds", RectangleValue(Rectangle(0.0, 50.0, 0.0, 50.0)),
                           "Distance", DoubleValue(10.0),
                           "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    ueMob.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                               "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                               "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    ueMob.Install(ueNodes);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    // Core Network/EPC and mmWave helpers
    Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper>();
    Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
    mmwaveHelper->SetEpcHelper(epcHelper);

    // Assign NR propagation settings if needed
    mmwaveHelper->SetAttribute("PathlossModel", StringValue("ns3::ThreeGppUmaPropagationLossModel"));
    mmwaveHelper->SetAttribute("ChannelModel", StringValue("ns3::MmWave3gppChannel"));

    // Install Devices
    NetDeviceContainer gnbDevs = mmwaveHelper->InstallGnbDevice(gnbNodes);
    NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);

    // Attach UEs to gNB
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        mmwaveHelper->Attach(ueDevs.Get(i), gnbDevs.Get(0));
    }

    // Assign IP Addresses
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set UE as default gateway to EPC
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Node> ueNode = ueNodes.Get(i);
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // UDP App: Server on UE 0, Client on UE 1
    uint16_t port = 5001;

    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(0.5));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper udpClient(ueIpIfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(1));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP Tracing
    mmwaveHelper->EnableTraces();
    epcHelper->EnablePcap("gnb-pcap", gnbDevs.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}