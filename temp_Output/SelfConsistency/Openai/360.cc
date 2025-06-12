#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create LTE and EPC helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // Create nodes: eNB and UE
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install Mobility Model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Install LTE devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack on UE and the EPC PGW node
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Gateway node gets named; extract PGW
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    internet.Install(pgw);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs), Ipv4AddressHelper("10.0.0.0", "255.255.255.0"));

    // Attach UE to the closest eNB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Enable default bearer for the UE
    enum EpsBearer::Qci qci = EpsBearer::NGBR_VIDEO_TCP_DEFAULT;
    EpsBearer bearer(qci);
    lteHelper->ActivateDedicatedEpsBearer(ueLteDevs, bearer, EpcTft::Default());

    // Set up applications:
    uint16_t udpEchoPort = 9;

    // Create UDP Echo Server application on the eNB's side, which is via PGW
    UdpEchoServerHelper echoServer(udpEchoPort);
    ApplicationContainer serverApps = echoServer.Install(pgw);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Allow time for interface setup
    Simulator::Schedule(Seconds(0.8), &Ipv4StaticRoutingHelper::RecomputeRoutes, Ipv4StaticRoutingHelper());

    // Get server address: the PGW's address as seen by the UE is 10.0.0.1 (first assigned)
    Ipv4Address serverAddress = ueIpIface.GetAddress(0); // Assign first address to UE; pgw is 10.0.0.1

    // Create UDP Echo Client on the UE to send requests to server at PGW
    UdpEchoClientHelper echoClient(Ipv4Address("10.0.0.1"), udpEchoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(80));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet capture
    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}