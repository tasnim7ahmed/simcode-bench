#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create LTE Helper
    LteHelper lteHelper;

    // Create eNB node
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Create UE node
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Configure the LTE network
    NetDeviceContainer enbDevs = lteHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper.InstallUeDevice(ueNodes);

    // Install the IP stack on the UE and eNB
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses to the UE and eNB
    Ipv4InterfaceContainer ueIpIface;
    Ipv4InterfaceContainer enbIpIface;

    Ipv4AddressHelper ip;
    ip.SetBase("10.1.1.0", "255.255.255.0");
    ueIpIface = ip.Assign(ueDevs);
    enbIpIface = ip.Assign(enbDevs);

    // Set up the UDP Echo Server on the eNB
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(enbNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the UDP Echo Client on the UE
    UdpEchoClientHelper echoClient(enbIpIface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Attach the UE to the eNB
    lteHelper.Attach(ueDevs, enbDevs.Get(0));

    // Activate a data radio bearer between UE and eNB
    enum EpsBearer::Qci q = EpsBearer::GBR_QCI_DEFAULT;
    EpsBearer bearer(q);
    lteHelper.ActivateDataRadioBearer(ueNodes.Get(0), bearer, enbDevs.Get(0));

    // Run the simulation
    Simulator::Run(Seconds(10.0));
    Simulator::Destroy();

    return 0;
}