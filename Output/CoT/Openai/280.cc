#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/nr-module.h"
#include "ns3/antenna-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes: 1 gNB and 2 UEs
    NodeContainer gNbNodes;
    gNbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Install Mobility for gNB (fixed)
    Ptr<ListPositionAllocator> gNbPositionAlloc = CreateObject<ListPositionAllocator>();
    gNbPositionAlloc->Add(Vector(25.0, 25.0, 5.0));
    MobilityHelper gNbMobility;
    gNbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gNbMobility.SetPositionAllocator(gNbPositionAlloc);
    gNbMobility.Install(gNbNodes);

    // Install Mobility for UEs (random walk)
    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator",
        "X", DoubleValue(25.0),
        "Y", DoubleValue(25.0),
        "rho", DoubleValue(24.5));
    ueMobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0.0, 50.0, 0.0, 50.0)),
        "Distance", DoubleValue(5.0)
    );
    ueMobility.Install(ueNodes);

    // Create NR Helper and Spectrum
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetAttribute("CellScanPeriod", TimeValue(Seconds(0.2)));

    // Use 3GPP channel model
    Ptr<ThreeGppSpectrumChannel> channel = CreateObject<ThreeGppSpectrumChannel>();
    Ptr<ThreeGppPropagationLossModel> lossModel = CreateObject<ThreeGppUmaPropagationLossModel>();
    channel->SetPropagationLossModel(lossModel);

    // Set up the simulation time
    double simTime = 10.0;

    // Configure spectrum: one band of 100 MHz at 28 GHz
    double centralFrequency = 28e9;
    double bandwidth = 100e6;
    BandwidthPartInfoPtrVector allBwps;
    uint8_t bwpId = 0;

    nrHelper->SetGnbDeviceAttribute("Numerology", UintegerValue(3));
    nrHelper->SetGnbDeviceAttribute("TransmissionBandwidth", UintegerValue(100));
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(1));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(1));
    nrHelper->InitializeOperationBandAndBwp();

    NetDeviceContainer gNbDevs = nrHelper->InstallGnbDevice(gNbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Attach UEs to gNB
    nrHelper->AttachToClosestEnb(ueDevs, gNbDevs);

    // Install the Internet stack (EPC function in NR)
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(gNbNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIfaces;
    ueIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set the default gateway for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNodes.Get(i)->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Configure applications
    uint16_t port = 8080;

    // TCP server on UE 1
    Address bindAddress(InetSocketAddress(ueIfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", bindAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(ueNodes.Get(1));
    serverApp.Start(Seconds(0.1));
    serverApp.Stop(Seconds(simTime));

    // TCP client on UE 0
    OnOffHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(ueIfaces.GetAddress(1), port));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(0.2)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(simTime)));

    ApplicationContainer clientApp = clientHelper.Install(ueNodes.Get(0));

    // Enable traces if needed
    // Packet::EnablePrinting ();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}