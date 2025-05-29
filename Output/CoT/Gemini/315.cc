#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/lte-module.h"
#include "ns3/config-store.h"
#include "ns3/epc-helper.h"
#include "ns3/lte-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Create the LTE Helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create the internet stack and routing
    InternetStackHelper internet;
    ueNodes.AddUplinkStack(internet);

    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    // Create the address
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignIpv4Address(NetDeviceContainer(ueDevs));
    Ipv4InterfaceContainer enbIpIface = epcHelper->AssignIpv4Address(NetDeviceContainer(enbDevs));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Attach the UE
    lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Activate a data radio bearer
    enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer(q);
    lteHelper->ActivateDedicatedEpsBearer(ueDevs.Get(0), bearer, enbDevs.Get(0));

    // Create the UDP application
    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1));
    serverApps.Stop(Seconds(10));

    UdpClientHelper client(ueIpIface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = client.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(2));
    clientApps.Stop(Seconds(10));

    // Mobility model
    Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel>();
    enbNodes.Get(0)->AggregateObject(enbMobility);
    enbMobility->SetPosition(Vector(0,0,0));

    Ptr<ConstantPositionMobilityModel> ueMobility = CreateObject<ConstantPositionMobilityModel>();
    ueNodes.Get(0)->AggregateObject(ueMobility);
    ueMobility->SetPosition(Vector(10,0,0));

    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}