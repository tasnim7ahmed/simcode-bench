#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-epc-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);
    LogComponentEnable("TcpServer", LOG_LEVEL_INFO);

    // Create nodes: 1 gNB, 2 UEs
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer gnbNode;
    gnbNode.Create(1);

    // Install Mobility for UE with random walk
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));

    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                              "Distance", DoubleValue(10.0),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                              "Mode", EnumValue(RandomWalk2dMobilityModel::MODE_TIME),
                              "Time", TimeValue(Seconds(1.0)));

    mobility.Install(ueNodes);

    // gNodeB is stationary at the center
    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMobility.Install(gnbNode);
    Ptr<MobilityModel> gnbMob = gnbNode.Get(0)->GetObject<MobilityModel>();
    gnbMob->SetPosition(Vector(25.0, 25.0, 0.0));

    // Install NR/EPC helpers
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetEpcHelper(epcHelper);

    // Spectrum configuration with default values
    nrHelper->SetNrPhyMacConfig("Frequency", DoubleValue(28e9), "Bandwidth", DoubleValue(100e6));

    // NR propagation loss set to Friis
    nrHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisPropagationLossModel"));

    // Install devices
    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNode);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Attach UE devices to gNB
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        nrHelper->Attach(ueDevs.Get(i), gnbDevs.Get(0));
    }

    // Install the IP stack on UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIfs = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set up static routing for UE
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Node> ueNode = ueNodes.Get(i);
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Set up server on UE1 (index 1)
    uint16_t serverPort = 8080;
    Address serverAddress(InetSocketAddress(ueIfs.GetAddress(1), serverPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    ApplicationContainer serverApp = packetSinkHelper.Install(ueNodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP client on UE0 (index 0)
    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer clientApp = clientHelper.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}