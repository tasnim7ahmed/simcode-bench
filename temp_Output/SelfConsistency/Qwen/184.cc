#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteSimpleWithNetAnim");

int main(int argc, char *argv[])
{
    // Enable logging for EPC-related components
    LogComponentEnable("EpcEnbApplication", LOG_LEVEL_INFO);
    LogComponentEnable("EpcPgwApplication", LOG_LEVEL_INFO);
    LogComponentEnable("EpcUeNas", LOG_LEVEL_INFO);

    // Set simulation time
    double simTime = 10.0;

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Create remote host node
    NodeContainer remoteHost;
    remoteHost.Create(1);

    // Install LTE and internet stack
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));  // eNodeB at origin
    positionAlloc->Add(Vector(10.0, 0.0, 0.0)); // UE1
    positionAlloc->Add(Vector(20.0, 0.0, 0.0)); // UE2
    positionAlloc->Add(Vector(100.0, 0.0, 0.0)); // Remote host

    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);
    mobility.Install(remoteHost);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Create point-to-point link between PGW and remote host
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    NetDeviceContainer internetDevices = p2ph.Install(remoteHost.Get(0), lteHelper->GetPgwNode());

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on UEs and remote host
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(remoteHost);

    Ipv4InterfaceContainer ueIpIfaces = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Assign IP addresses to remote host
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer remoteIpIfaces = ipv4.Assign(internetDevices);

    // Attach UEs to the first eNodeB (index 0)
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Install UDP echo server on remote host
    uint16_t dlPort = 1234;
    UdpEchoServerHelper echoServer(dlPort);
    ApplicationContainer serverApps = echoServer.Install(remoteHost.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Install UDP echo clients on UEs
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        UdpEchoClientHelper echoClient(remoteIpIfaces.GetAddress(0), dlPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(100));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(u));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(simTime));
    }

    // Enable NetAnim tracing
    AnimationInterface anim("lte-simple-netanim.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("lte-route-trace.xml", Seconds(0), Seconds(simTime), Seconds(0.5));

    // Set simulation stop time
    Simulator::Stop(Seconds(simTime));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}