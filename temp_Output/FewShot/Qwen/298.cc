#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for handover and mobility
    LogComponentEnable("LteHelper", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdmIpv4PdpContext", LOG_LEVEL_INFO);

    // Set simulation time
    double simTime = 10.0;
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));

    // Create LTE helper components
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");

    // Create nodes: 2 eNodeBs and 1 UE
    NodeContainer enbNodes;
    enbNodes.Create(2);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install LTE devices
    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UE to a random eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Set up UDP server on first eNodeB
    uint16_t dlPort = 1234;
    UdpEchoServerHelper echoServer(dlPort);
    ApplicationContainer serverApp = echoServer.Install(enbNodes.Get(0));
    serverApp.Start(Seconds(0.01));
    serverApp.Stop(Seconds(simTime));

    // Set up UDP client on the UE
    UdpEchoClientHelper echoClient(ueIpIfaces.GetAddress(0), dlPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(0.01));
    clientApp.Stop(Seconds(simTime));

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, 0, 50)),
                              "Distance", DoubleValue(5.0));
    mobility.Install(ueNodes);

    // EnodeB positions
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(0.0, 0.0, 0.0));   // eNB 0
    enbPositionAlloc->Add(Vector(100.0, 0.0, 0.0)); // eNB 1

    mobility.SetPositionAllocator(enbPositionAlloc);
    mobility.Install(enbNodes);

    // Enable handover
    lteHelper->AddX2Interface(enbNodes);
    lteHelper->HandoverRequest(Seconds(5.0), ueDevs.Get(0), enbNodes.Get(0), enbNodes.Get(1));

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}