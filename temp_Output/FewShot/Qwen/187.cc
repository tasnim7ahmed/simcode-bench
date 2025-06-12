#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/fattree-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Fat-Tree configuration parameters
    uint32_t k = 4;  // Fat-Tree parameter (even number, typically 4 or 8)
    uint32_t hostPerPod = (k / 2);  // Number of hosts per pod

    // Create Fat-Tree topology helper
    FatTreeHelper fattree(k);

    // Create nodes and install the Internet stack
    NodeContainer allNodes = fattree.CreateAndInstallStack();

    // Assign IP addresses to all devices
    Ipv4AddressHelper ip;
    ip.SetBase("10.1.0.0", "255.255.0.0");
    fattree.AssignIpv4Addresses(ip);

    // Set up TCP connections between random pairs of hosts
    uint16_t port = 5000;
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    // Get host nodes
    std::vector<NodeContainer> hosts = fattree.GetHostContainers();

    // Setup servers and clients on random host pairs
    for (uint32_t i = 0; i < hosts.size(); ++i) {
        for (uint32_t j = 0; j < hosts[i].GetN(); ++j) {
            if ((i == 0 && j == 0)) { // pick one source node
                Ptr<Node> sender = hosts[i].Get(j);
                for (uint32_t x = 0; x < hosts.size(); ++x) {
                    for (uint32_t y = 0; y < hosts[x].GetN(); ++y) {
                        if ((x != i || y != j)) { // different than sender
                            Ptr<Node> receiver = hosts[x].Get(y);
                            // Install TCP Server on receiver
                            PacketSinkHelper sink("ns3::TcpSocketFactory",
                                                  InetSocketAddress(Ipv4Address::GetAny(), port));
                            ApplicationContainer serverApp = sink.Install(receiver);
                            serverApp.Start(Seconds(1.0));
                            serverApp.Stop(Seconds(10.0));

                            // Install TCP Client on sender
                            BulkSendHelper senderHelper("ns3::TcpSocketFactory",
                                                        InetSocketAddress(fattree.GetIpAddress(x, y), port));
                            senderHelper.SetAttribute("MaxBytes", UintegerValue(1000000));
                            ApplicationContainer clientApp = senderHelper.Install(sender);
                            clientApp.Start(Seconds(2.0));
                            clientApp.Stop(Seconds(10.0));

                            clientApps.Add(clientApp);
                            serverApps.Add(serverApp);
                            break; // Only connect to one destination for simplicity
                        }
                    }
                    if (clientApps.GetN() > 0) break;
                }
                break; // Only run once
            }
        }
        if (clientApps.GetN() > 0) break;
    }

    // Enable NetAnim visualization
    AnimationInterface anim("fattree.xml");
    anim.EnablePacketMetadata(true);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}