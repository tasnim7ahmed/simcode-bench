#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteHandoverSimulation");

int main(int argc, char *argv[])
{
    // Enable logging components
    LogComponentEnable("LteHandoverSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClientServerApplication", LOG_LEVEL_INFO);

    // Simulation time
    double simTime = 10.0;
    Config::SetDefault("ns3::ConfigStore::Filename", StringValue("input-defaults.txt"));
    Config::SetDefault("ns3::ConfigStore::Mode", StringValue("Load"));

    // Create eNodeBs and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(2);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install LTE devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");

    // PointToPoint links between eNBs and EPC
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Set pathloss model
    lteHelper->SetPathlossModelType(TypeId::LookupByName("ns3::FriisSpectrumPropagationLossModel"));

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install internet stack on UE
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UE to a random eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Setup UDP server on first eNodeB
    uint16_t dlPort = 1234;
    UdpServerHelper dlServer(dlPort);
    ApplicationContainer dlServerApp = dlServer.Install(enbNodes.Get(0));
    dlServerApp.Start(Seconds(0.0));
    dlServerApp.Stop(Seconds(simTime));

    // Setup UDP client on UE
    UdpClientHelper dlClient(ueIpIfaces.GetAddress(0), dlPort);
    dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    dlClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    dlClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer dlClientApp = dlClient.Install(ueNodes.Get(0));
    dlClientApp.Start(Seconds(2.0));
    dlClientApp.Stop(Seconds(simTime));

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 150, -50, 50)),
                              "Distance", DoubleValue(5.0));
    mobility.Install(ueNodes);

    // Output positions every second
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> positionStream = asciiTraceHelper.CreateFileStream("ue-positions.txt");
    ueNodes.Get(0)->GetObject<MobilityModel>()->TraceConnectWithoutContext("CourseChange", MakeBoundCallback(&CourseChange, positionStream));

    // Enable handover
    lteHelper->AddX2Interface(enbNodes);
    lteHelper->EnableHandover();

    // Enable tracing
    lteHelper->EnablePhyTraces();
    lteHelper->EnableMacTraces();
    lteHelper->EnableRlcTraces();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}