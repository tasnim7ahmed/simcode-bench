#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteSingleCellSimulation");

int main(int argc, char *argv[])
{
    // Enable logging components if needed
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));
    Config::SetDefault("ns3::LteHelper::UseIdealRrcSap", BooleanValue(true));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install Mobility Model for UE
    Ptr<RandomWalk2dMobilityModel> mobility = CreateObject<RandomWalk2dMobilityModel>();
    mobility->SetAttribute("Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility->SetAttribute("Distance", DoubleValue(5.0));
    mobility->SetAttribute("Speed", RandomVariableStreamValue(UniformRandomVariable(0.5, 2.0)));
    ueNodes.Get(0)->AggregateObject(mobility);

    NetDeviceContainer enbDevs;
    NetDeviceContainer ueDevs;

    enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to the nearest eNB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Set up applications
    uint16_t port = 9; // UDP Echo port

    // Server (on UE)
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Client (on eNodeB)
    UdpEchoClientHelper echoClient(ueIpIface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}