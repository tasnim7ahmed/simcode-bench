#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

    // Enable logging for LTE module
    LogComponentEnable("LteEnbNetDevice", LOG_LEVEL_INFO);
    LogComponentEnable("LteUeNetDevice", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create LTE Helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Create eNodeB node
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Create UE nodes
    NodeContainer ueNodes;
    ueNodes.Create(3);

    // Configure Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("50.0"),
                                  "Y", StringValue("50.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(ueNodes);

    //Set eNB fixed position
    Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel>();
    enbNodes.Get(0)->AggregateObject(enbMobility);
    Vector enbPosition(50.0, 50.0, 0.0);
    enbMobility->SetPosition(enbPosition);

    // Install LTE devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UEs to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));
    lteHelper->Attach(ueLteDevs.Get(1), enbLteDevs.Get(0));
    lteHelper->Attach(ueLteDevs.Get(2), enbLteDevs.Get(0));

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to the UE nodes
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);

    // Install Applications
    // Setup UDP server on UE 0
    uint16_t port = 5000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP client on UE 1
    UdpClientHelper client(ueIpIface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4000)); // 10 seconds / 10ms
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = client.Install(ueNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Add a default EPS bearer
    enum EpsBearer::Qci q = EpsBearer::GBR_QCI_DEFAULT;
    EpsBearer bearer(q);
    lteHelper->ActivateDedicatedEpsBearer(ueLteDevs.Get(0), bearer, TdmaSpectrumValueHelper::Default());
    lteHelper->ActivateDedicatedEpsBearer(ueLteDevs.Get(1), bearer, TdmaSpectrumValueHelper::Default());
    lteHelper->ActivateDedicatedEpsBearer(ueLteDevs.Get(2), bearer, TdmaSpectrumValueHelper::Default());

    // Enable PCAP Tracing
    lteHelper->EnableTraces();

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Simulator::Destroy();
    return 0;
}