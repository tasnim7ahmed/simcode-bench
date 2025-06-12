#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NrTcpSimulation");

int main(int argc, char *argv[])
{
    // Log components
    LogComponentEnable("NrTcpSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("NrUePhy", LOG_LEVEL_INFO);

    // Simulation parameters
    double simTime = 10.0;
    uint32_t gNbNum = 1;
    uint32_t ueNum = 2;

    // Create nodes
    NodeContainer gnbNodes;
    gnbNodes.Create(gNbNum);

    NodeContainer ueNodes;
    ueNodes.Create(ueNum);

    // Mobility configuration
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    mobility.Install(ueNodes);

    // Install NR stack
    NrHelper nrHelper;

    // Set spectrum model
    BandwidthPartInfoPtrVector allBwps;
    auto dlBand = NrSpectrumValueHelper::CreateDlSpectrumModel(7.11e9, 100e6);
    auto ulBand = NrSpectrumValueHelper::CreateUlSpectrumModel(7.11e9, 100e6);

    allBwps.push_back(Create<BandwidthPartInfo>(dlBand));
    allBwps.push_back(Create<BandwidthPartInfo>(ulBand));

    nrHelper.SetChannelConditionModelType("ns3::ThreeGppUmiStreetCanyonChannelConditionModel");
    nrHelper.SetPathlossModelType("ns3::ThreeGppUmiStreetCanyonPropagationLossModel");

    NetDeviceContainer enbNetDev = nrHelper.InstallGnbDevice(gnbNodes, allBwps);
    NetDeviceContainer ueNetDev = nrHelper.InstallUeDevice(ueNodes, allBwps);

    // Activate data radio bearers
    nrHelper.AttachToEnb(ueNetDev, enbNetDev);

    // Install the IP stack
    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueInterfaces;
    Ipv4AddressHelper ipv4Addr;

    ipv4Addr.SetBase("192.168.0.0", "255.255.255.0");
    ueInterfaces = ipv4Addr.Assign(ueNetDev);

    // Setup TCP server and client
    uint16_t sinkPort = 8080;

    // Server (UE 1)
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(ueNodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime));

    // Client (UE 0)
    Address sinkAddress(InetSocketAddress(ueInterfaces.GetAddress(1), sinkPort));
    BulkSendHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // send forever
    ApplicationContainer clientApp = clientHelper.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}