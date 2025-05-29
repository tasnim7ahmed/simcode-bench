#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/random-walk-2d-mobility-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpRandomWalk");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("LteUdpRandomWalk", LOG_LEVEL_INFO);

    // Simulation parameters
    uint16_t numberOfUes = 3;
    double simulationTime = 10.0;
    uint16_t dlEarfcn = 500;

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Set eNodeB attributes
    lteHelper->SetEnbDeviceAttribute("DlEarfcn", UintegerValue(dlEarfcn));
    lteHelper->SetEnbDeviceAttribute("UlEarfcn", UintegerValue(dlEarfcn + 18000));

    // Create EPC helper
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create remote host
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create internet stack helper
    EpcTlvMapping tlvMapping;
    epcHelper->SetTlvMapping(tlvMapping);
    Address remoteHostAddr;
    remoteHostAddr = epcHelper->AllocateIpv4Address();
    Ipv4InterfaceContainer internetIpIfaces = epcHelper->AssignIpv4Address(NetDeviceContainer(epcHelper->GetNetDevice(remoteHost)));
    remoteHostAddr = internetIpIfaces.GetAddress(0);

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(numberOfUes);

    // Install LTE devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignIpv4Address(ueLteDevs);

    // Attach UEs to the closest eNodeB
    for (uint16_t i = 0; i < numberOfUes; i++) {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Activate a data radio bearer between UE and eNodeB
    enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer(q);
    for (uint16_t i = 0; i < numberOfUes; i++) {
        lteHelper->ActivateDataRadioBearer(ueNodes.Get(i), bearer);
    }

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.Install(ueNodes);

    // Install server on the eNodeB
    UdpServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(remoteHost);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // Install client on the UEs
    UdpClientHelper echoClient(remoteHostAddr, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(ueNodes);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    // Configure tracing
    // Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>("lte-udp-random-walk.pcap", std::ios::out);
    // lteHelper->EnablePcapAll(stream);

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Cleanup
    Simulator::Destroy();

    return 0;
}