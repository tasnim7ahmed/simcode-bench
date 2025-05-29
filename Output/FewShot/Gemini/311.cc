#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create LTE Helper
    LteHelper lteHelper;

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Create EPC Helper
    Ptr<LteEnbNetDevice> enbLteDev = lteHelper.InstallEnbDevice(enbNodes.Get(0));
    NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

    // Create EPC Helper
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper.SetEpcHelper(epcHelper);

    // Assign IP addresses to UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface;

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        lteHelper.Attach(ueLteDevs.Get(i), enbLteDev);
    }

    // Install internet stack on eNodeB
    internet.Install(enbNodes);

    // Create a single address space
    Ipv4AddressHelper ipAddrs;
    ipAddrs.SetBase("10.1.1.0", "255.255.255.0");
    ueIpIface = ipAddrs.Assign(ueLteDevs);

    // Install TCP server on UE 0
    uint16_t port = 8080;
    Address serverAddress (InetSocketAddress (ueIpIface.GetAddress (0), port));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApps = packetSinkHelper.Install (ueNodes.Get (0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    // Install TCP client on UE 1
    BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (ueIpIface.GetAddress (0), port));
    source.SetAttribute ("MaxBytes", UintegerValue (1000000));
    ApplicationContainer clientApps = source.Install (ueNodes.Get (1));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    // Activate default EPS bearer
    lteHelper.ActivateDedicatedEpsBearer (ueLteDevs.Get(0), EpsBearer(EpsBearer::GBR_CONV_VOICE));
    lteHelper.ActivateDedicatedEpsBearer (ueLteDevs.Get(1), EpsBearer(EpsBearer::GBR_CONV_VOICE));


    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}