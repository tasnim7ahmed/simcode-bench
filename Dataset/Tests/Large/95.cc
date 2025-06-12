void TestReadNxNMatrix()
{
    // Prepare a mock adjacency matrix file
    std::string testFileName = "test_adjacency_matrix.txt";
    std::ofstream outFile(testFileName);
    outFile << "0 1 0\n";
    outFile << "1 0 1\n";
    outFile << "0 1 0\n";
    outFile.close();
    
    // Read the matrix using the function
    std::vector<std::vector<bool>> matrix = readNxNMatrix(testFileName);
    
    // Test the expected results
    NS_ASSERT(matrix.size() == 3);
    NS_ASSERT(matrix[0][1] == true);
    NS_ASSERT(matrix[1][2] == true);
    NS_ASSERT(matrix[2][0] == false);

    // Cleanup
    std::remove(testFileName.c_str());
}

void TestReadCoordinatesFile()
{
    // Prepare a mock coordinates file
    std::string testFileName = "test_node_coordinates.txt";
    std::ofstream outFile(testFileName);
    outFile << "1.0 2.0\n";
    outFile << "2.0 3.0\n";
    outFile << "3.0 4.0\n";
    outFile.close();
    
    // Read the coordinates using the function
    std::vector<std::vector<double>> coordinates = readCoordinatesFile(testFileName);
    
    // Test the expected results
    NS_ASSERT(coordinates.size() == 3);
    NS_ASSERT(coordinates[0][0] == 1.0);
    NS_ASSERT(coordinates[1][1] == 3.0);
    
    // Cleanup
    std::remove(testFileName.c_str());
}

void TestPrintMatrix()
{
    std::vector<std::vector<bool>> testMatrix = {{0, 1}, {1, 0}};
    printMatrix("Test Matrix", testMatrix);
    // Expectation: The function should execute without errors and print the matrix to the console
}

void TestPrintCoordinateArray()
{
    std::vector<std::vector<double>> testCoordinates = {{1.0, 2.0}, {3.0, 4.0}};
    printCoordinateArray("Test Coordinates", testCoordinates);
    // Expectation: The function should execute without errors and print the coordinates to the console
}

void TestNetworkSetup()
{
    // Prepare mock adjacency matrix and coordinates
    std::string adjMatFile = "test_adj_matrix.txt";
    std::string coordFile = "test_coordinates.txt";
    std::ofstream adjMatOutFile(adjMatFile);
    adjMatOutFile << "0 1\n1 0\n";
    adjMatOutFile.close();
    
    std::ofstream coordOutFile(coordFile);
    coordOutFile << "0 0\n1 1\n";
    coordOutFile.close();
    
    // Read adjacency matrix and coordinates
    std::vector<std::vector<bool>> adjMatrix = readNxNMatrix(adjMatFile);
    std::vector<std::vector<double>> coordinates = readCoordinatesFile(coordFile);
    
    // Simulate the network setup with nodes
    int n_nodes = coordinates.size();
    NodeContainer nodes;
    nodes.Create(n_nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    Ipv4AddressHelper ipv4_n;
    ipv4_n.SetBase("10.0.0.0", "255.255.255.252");
    
    int linkCount = 0;
    for (size_t i = 0; i < adjMatrix.size(); i++)
    {
        for (size_t j = 0; j < adjMatrix[i].size(); j++)
        {
            if (adjMatrix[i][j])
            {
                NodeContainer n_links = NodeContainer(nodes.Get(i), nodes.Get(j));
                NetDeviceContainer n_devs = p2p.Install(n_links);
                ipv4_n.Assign(n_devs);
                ipv4_n.NewNetwork();
                linkCount++;
            }
        }
    }

    // Test the expected number of links
    NS_ASSERT(linkCount == 1); // Should be 1 based on the matrix

    // Cleanup
    std::remove(adjMatFile.c_str());
    std::remove(coordFile.c_str());
}

void TestSimulationSetup()
{
    // Prepare mock adjacency matrix and coordinates
    std::string adjMatFile = "test_adj_matrix.txt";
    std::string coordFile = "test_coordinates.txt";
    std::ofstream adjMatOutFile(adjMatFile);
    adjMatOutFile << "0 1\n1 0\n";
    adjMatOutFile.close();
    
    std::ofstream coordOutFile(coordFile);
    coordOutFile << "0 0\n1 1\n";
    coordOutFile.close();
    
    // Read adjacency matrix and coordinates
    std::vector<std::vector<bool>> adjMatrix = readNxNMatrix(adjMatFile);
    std::vector<std::vector<double>> coordinates = readCoordinatesFile(coordFile);
    
    // Simulate the network setup
    int n_nodes = coordinates.size();
    NodeContainer nodes;
    nodes.Create(n_nodes);
    
    // Test basic simulation setup (no traffic flows or routing)
    Simulator::Stop(Seconds(1));
    Simulator::Run();
    Simulator::Destroy();
    
    // Cleanup
    std::remove(adjMatFile.c_str());
    std::remove(coordFile.c_str());
}

